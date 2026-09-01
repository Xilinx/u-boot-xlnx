// SPDX-License-Identifier: GPL-2.0
/*
 * Synopsys DesignWare DisplayPort TX (DPTX) core
 *
 * Generic register-level engine -- see dw_dp_core.h for the module
 * boundary. This file has no driver-model registration of its own (no
 * U_BOOT_DRIVER, no uclass, no of_match): it is a plain linked-in library
 * that vendor glue drivers call into directly.
 *
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 * Author: Pranav Sanwal <pranav.sanwal@amd.com>
 */

#define LOG_CATEGORY UCLASS_DISPLAY

#include <errno.h>
#include <log.h>

#include <dm/device.h>
#include <dm/device_compat.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/string.h>

#include "dw_dp_core.h"

/*
 * Nominal link clock, in kHz, per PHY rate index (0=RBR/1=HBR/2=HBR2)
 * -- used only for DPTX_VIDEO_HBLANK_INTERVAL, independent of lane count.
 */
static const u32 dw_dp_link_clk_khz[] = {
	[0] = 40500,	/* RBR  */
	[1] = 67500,	/* HBR  */
	[2] = 135000,	/* HBR2 */
};

/*
 * 5000 x 1us = 5ms, comfortably above the hardware's documented worst-case
 * AUX reply timeout of 3.2ms (AUX_STATUS.AUX_TIMEOUT).
 */
#define AUX_MAX_RETRIES		5000
#define HPD_POLL_RETRIES	50
#define PHY_BUSY_RETRIES	20

/*
 * Whole-transaction retries for DEFER / zero-byte-ACK / invalid status, and
 * for a sink that has not replied at all yet -- 1ms between attempts, so
 * 200 retries is ~200ms worst case. Some sinks need a brief settle time
 * after HPD assertion before their AUX responder is ready; the original 80
 * (~80ms) was occasionally too short for the very first post-attach AUX
 * transaction (the diagnostic DPCD capability read), even though the link
 * still trained successfully afterwards.
 */
#define AUX_XFER_RETRIES	200

/*
 * CONFIG5 must be written in two steps with this gap between them; the
 * reference driver requires the same delay pattern for reliable lock.
 */
#define CONFIG5_WRITE_GAP_MS	150

/* AUX-only soft-reset pulse width. */
#define AUX_SOFT_RESET_PULSE_US		10
/* PHY_BUSY poll interval, bounded by PHY_BUSY_RETRIES. */
#define PHY_BUSY_POLL_US		500
/* Settle time after each SET_POWER transition in dw_dp_core_start_video_stream(). */
#define SET_POWER_D3_SETTLE_MS		10
#define SET_POWER_D0_SETTLE_MS		30
/* Settle time after the full controller soft reset in dw_dp_core_attach(). */
#define ATTACH_SOFT_RESET_SETTLE_MS	10
/* Sink wake-up settle time after each SET_POWER transition in dw_dp_core_attach(). */
#define SINK_D3_WAKE_MS			100
#define SINK_D0_SETTLE_MS		50
/* Re-check delay after clearing latched FIFO fault bits, to catch a recurring fault. */
#define FIFO_FAULT_RECHECK_MS		50

/**
 * aux_soft_reset() - pulse the AUX-only soft reset
 * @core: core state
 *
 * Used to recover from an ACK-with-zero-bytes or invalid AUX reply, both
 * hardware quirks that leave the AUX engine wedged until reset.
 */
static void aux_soft_reset(struct dw_dp *core)
{
	dw_dp_wr(core, DPTX_SOFT_RESET, DPTX_SRST_AUX);
	udelay(AUX_SOFT_RESET_PULSE_US);
	dw_dp_wr(core, DPTX_SOFT_RESET, 0);
}

/**
 * aux_wait_reply() - poll AUX_STATUS until the current transaction completes
 * @core: core state
 * @status_out: set to the final AUX_STATUS value, whether this call
 *		succeeds or fails
 *
 * Called from aux_transfer()'s retry loop, up to AUX_XFER_RETRIES times per
 * transfer -- deliberately does not log anything itself (unlike a one-shot
 * helper, an unconditional dev_err() here would repeat on every outer retry).
 * The caller aggregates failures into its own single end-of-transfer summary.
 *
 * Return: 0 if the transaction completed without a HW timeout or reply
 *	   error, -ETIMEDOUT or -EIO otherwise.
 */
static int aux_wait_reply(struct dw_dp *core, u32 *status_out)
{
	u32 status;
	int i;

	for (i = 0; i < AUX_MAX_RETRIES; i++) {
		status = dw_dp_rd(core, DPTX_AUX_STATUS);
		if (!(status & DPTX_AUX_BUSY))
			break;
		if (status & DPTX_AUX_TIMEOUT) {
			*status_out = status;
			return -ETIMEDOUT;
		}
		udelay(1);
	}

	*status_out = status;

	if (status & DPTX_AUX_BUSY)
		return -ETIMEDOUT;
	if (status & DPTX_AUX_TIMEOUT)
		return -ETIMEDOUT;
	if (status & DPTX_AUX_REPLY_ERR)
		return -EIO;

	return 0;
}

/**
 * aux_transfer() - issue one native AUX transaction, with retries
 * @core: core state
 * @is_read: true for a read, false for a write
 * @addr: DPCD address
 * @len: transfer length in bytes (1-16)
 * @wdata: buffer to write from, used when @is_read is false
 * @rdata: buffer to read into, used when @is_read is true
 *
 * Retries transient failures: DEFER, and an ACK with zero reply bytes or an
 * invalid reply status (both a known hardware quirk that requires an
 * AUX-only soft reset to clear). All three share the same AUX_XFER_RETRIES
 * budget -- real hardware has been observed needing well over a handful of
 * soft resets to recover, so a separate, smaller cap for the soft-reset
 * quirk is not safe; it turns an occasional slow-but-working transfer into
 * an intermittent hard failure instead.
 *
 * Per-retry-category counts are tracked in plain variables rather than
 * logged as they happen -- this loop can run up to AUX_XFER_RETRIES times
 * per call, and a dev_dbg() per iteration turns a ~1ms-per-try loop into
 * one bottlenecked on serial console throughput once debug logging is
 * enabled, which can be much slower than the timing this retry loop
 * assumes. A single summary line is logged once the transfer concludes.
 *
 * Return: 0 if OK, -ve on error.
 */
static int aux_transfer(struct dw_dp *core, bool is_read, u32 addr,
			u8 len, const u8 *wdata, u8 *rdata)
{
	unsigned int wait_failed_n = 0, defer_n = 0, soft_reset_n = 0;
	u32 buf[4] = {0};
	int reply, tries;
	u32 cmd, status;
	unsigned int br;

	cmd = (is_read ? DPTX_AUX_CMD_NATIVE_READ : DPTX_AUX_CMD_NATIVE_WRITE) |
	      ((addr & DPTX_AUX_CMD_ADDR_MASK) << DPTX_AUX_CMD_ADDR_SHIFT) |
	      ((len - 1) & DPTX_AUX_CMD_LEN_MASK);

	for (tries = 0; tries < AUX_XFER_RETRIES; tries++) {
		if (tries)
			mdelay(1);

		if (!is_read)
			memcpy(buf, wdata, len > DPTX_AUX_MAX_LEN ? DPTX_AUX_MAX_LEN : len);

		dw_dp_wr(core, DPTX_AUX_DATA0, buf[0]);
		dw_dp_wr(core, DPTX_AUX_DATA1, buf[1]);
		dw_dp_wr(core, DPTX_AUX_DATA2, buf[2]);
		dw_dp_wr(core, DPTX_AUX_DATA3, buf[3]);
		dw_dp_wr(core, DPTX_AUX_CMD, cmd);

		reply = aux_wait_reply(core, &status);
		if (reply) {
			wait_failed_n++;
			continue;	/* transient -- retry the whole transaction */
		}

		br = (status & DPTX_AUX_BYTES_READ_MASK) >> DPTX_AUX_BYTES_READ_SHIFT;

		switch ((status & DPTX_AUX_REPLY_CODE_MASK) >> DPTX_AUX_REPLY_CODE_SHIFT) {
		case DPTX_AUX_REPLY_ACK:
			if (!br) {
				aux_soft_reset(core);
				soft_reset_n++;
				continue;
			}
			if (is_read) {
				buf[0] = dw_dp_rd(core, DPTX_AUX_DATA0);
				buf[1] = dw_dp_rd(core, DPTX_AUX_DATA1);
				buf[2] = dw_dp_rd(core, DPTX_AUX_DATA2);
				buf[3] = dw_dp_rd(core, DPTX_AUX_DATA3);
				memcpy(rdata, buf, len > DPTX_AUX_MAX_LEN ? DPTX_AUX_MAX_LEN : len);
			}
			if (tries)
				dev_dbg(core->dev,
					"DPTX: AUX %s addr=0x%x: ok after %d retries (wf=%u df=%u rst=%u)\n",
					is_read ? "read" : "write", addr, tries,
					wait_failed_n, defer_n, soft_reset_n);
			return 0;
		case DPTX_AUX_REPLY_NACK:
			dev_err(core->dev, "AUX %s addr=0x%x len=%u: NACK, status=0x%08x\n",
				is_read ? "read" : "write", addr, len, status);
			return -EIO;
		case DPTX_AUX_REPLY_DEFER:
			defer_n++;
			continue;
		default:
			aux_soft_reset(core);
			soft_reset_n++;
			continue;
		}
	}
	dev_err(core->dev,
		"AUX %s addr=0x%x len=%u: exhausted %d retries (%u wait-failed, %u deferred, %u soft-reset)\n",
		is_read ? "read" : "write", addr, len, AUX_XFER_RETRIES,
		wait_failed_n, defer_n, soft_reset_n);

	return -ETIMEDOUT;
}

static int aux_write(struct dw_dp *core, u32 addr, u8 len, const u8 *data)
{
	return aux_transfer(core, false, addr, len, data, NULL);
}

static int aux_read(struct dw_dp *core, u32 addr, u8 len, u8 *data)
{
	return aux_transfer(core, true, addr, len, NULL, data);
}

/**
 * dpcd_bw_to_phy_rate() - convert a DPCD link bandwidth code to the 2-bit
 *			    hardware rate field
 * @bw: DPCD link bandwidth code (e.g. DP_LINK_BW_1_62)
 *
 * PHYIF_CTRL[5:4] expects 0=RBR, 1=HBR, 2=HBR2, not the raw DPCD code.
 *
 * Return: the hardware rate field value.
 */
static u8 dpcd_bw_to_phy_rate(u8 bw)
{
	switch (bw) {
	case DP_LINK_BW_1_62:
		return DW_DP_PHY_RATE_RBR;
	case DP_LINK_BW_2_7:
		return DW_DP_PHY_RATE_HBR;
	case DP_LINK_BW_5_4:
		return DW_DP_PHY_RATE_HBR2;
	default:
		/* unreachable: only hardcoded valid DPCD codes are passed in */
		return DW_DP_PHY_RATE_RBR;
	}
}

const char *dw_dp_link_rate_name(u8 link_rate)
{
	switch (link_rate) {
	case DP_LINK_BW_1_62:
		return "RBR";
	case DP_LINK_BW_2_7:
		return "HBR";
	case DP_LINK_BW_5_4:
		return "HBR2";
	default:
		return "unknown";
	}
}

/**
 * lane_count_to_phy_lanes() - convert a lane count to the PHY_LANES index
 * @lanes: lane count (1, 2, or 4)
 *
 * PHYIF_CTRL[7:6] (PHY_LANES) is an encoded index, not the raw lane count:
 * 1 lane -> 0, 2 lanes -> 1, 4 lanes -> 2. Passing the raw count instead
 * makes the hardware believe more lanes are active than actually have
 * XMIT_EN set, and the unused lanes' PHY_BUSY bits never clear.
 *
 * Return: the PHY_LANES index, or 0 (with a debug log) for an unexpected
 *	   lane count.
 */
static u8 lane_count_to_phy_lanes(u8 lanes)
{
	switch (lanes) {
	case 1:
		return 0;
	case 2:
		return 1;
	case 4:
		return 2;
	default:
		debug("DPTX: unexpected lane_count=%u, defaulting to 1 lane\n",
		      lanes);
		return 0;
	}
}

/**
 * phy_wait_busy_clear() - poll PHYIF_CTRL.PHY_BUSY until it clears
 * @core: core state
 *
 * Every PHYIF_CTRL write (power sequencing, pattern/rate/lane changes) needs
 * the PHY to finish applying it before the next register write is valid.
 *
 * Return: 0 once PHY_BUSY clears, -ETIMEDOUT if PHY_BUSY_RETRIES is exhausted.
 */
static int phy_wait_busy_clear(struct dw_dp *core)
{
	u32 val;
	int i;

	for (i = 0; i < PHY_BUSY_RETRIES; i++) {
		val = dw_dp_rd(core, DPTX_PHYIF_CTRL);
		if (!(val & DPTX_PHY_BUSY_MASK))
			return 0;
		udelay(PHY_BUSY_POLL_US);
	}
	dev_err(core->dev, "PHY_BUSY never cleared, PHYIF_CTRL=0x%08x\n", val);

	return -ETIMEDOUT;
}

/**
 * phy_set_pattern() - program the PHY's per-lane test pattern, rate, and lanes
 * @core: core state, with lane_count/phy_rate already set
 * @tps: training pattern to select (DP_TPS_NONE/DP_TPS1/DP_TPS2/DP_TPS3)
 *
 * Sets XMIT_EN on every active lane and waits for the PHY to apply the
 * change.
 *
 * Return: 0 if OK, -ve on error.
 */
static int phy_set_pattern(struct dw_dp *core, u8 tps)
{
	u8 phy_lanes;
	u32 val;
	int i;

	phy_lanes = lane_count_to_phy_lanes(core->lane_count);
	val = DPTX_PHY_TPS(tps) | DPTX_PHY_RATE(core->phy_rate)
	    | DPTX_PHY_LANES(phy_lanes);
	for (i = 0; i < core->lane_count; i++)
		val |= DPTX_PHY_XMIT_EN(i);

	dev_dbg(core->dev, "%s: tps=%u phy_rate=%u lane_count=%u, PHYIF_CTRL=0x%08x\n",
		__func__, tps, core->phy_rate, core->lane_count, val);
	dw_dp_wr(core, DPTX_PHYIF_CTRL, val);

	return phy_wait_busy_clear(core);
}

/**
 * phy_power_sequence() - bring the attached PHY out of full power-down
 * @core: core state
 *
 * PHYIF_CTRL[20:17] (PHY_POWERDOWN) resets to 3 (full power-down). Neither
 * AUX nor the main link will respond until the PHY is brought up through
 * the required 3 -> 2 -> 0 power sequence, waiting for PHY_BUSY to clear
 * between each step.
 *
 * Return: 0 if OK, -ve on error.
 */
static int phy_power_sequence(struct dw_dp *core)
{
	int ret;

	dw_dp_wr(core, DPTX_PHYIF_CTRL, DPTX_PHY_POWERDOWN(DPTX_PHY_POWERDOWN_FULL));
	ret = phy_wait_busy_clear(core);
	if (ret)
		return ret;
	mdelay(1);

	dw_dp_wr(core, DPTX_PHYIF_CTRL, DPTX_PHY_POWERDOWN(DPTX_PHY_POWERDOWN_P2));
	ret = phy_wait_busy_clear(core);
	if (ret)
		return ret;
	mdelay(1);

	dw_dp_wr(core, DPTX_PHYIF_CTRL, DPTX_PHY_POWERDOWN(DPTX_PHY_POWERDOWN_ON));

	return phy_wait_busy_clear(core);
}

/**
 * dw_dp_wait_hpd() - poll HPD_STATUS for a connected sink
 * @core: core state
 *
 * Return: 0 once HPD_LINE is asserted, -ENODEV if HPD_POLL_RETRIES is
 *	   exhausted (no sink connected).
 */
static int dw_dp_wait_hpd(struct dw_dp *core)
{
	u32 val;
	int i;

	for (i = 0; i < HPD_POLL_RETRIES; i++) {
		val = dw_dp_rd(core, DPTX_HPD_STATUS);
		if (val & DPTX_HPD_LINE)
			return 0;
		mdelay(1);
	}
	dev_info(core->dev, "DPTX: display not connected\n");
	dev_dbg(core->dev, "HPD not asserted, HPD_STATUS=0x%08x\n", val);

	return -ENODEV;
}

/**
 * dw_dp_read_adjust_request() - read the sink's requested per-lane drive
 *				  settings
 * @core: core state
 * @lane_count: number of lanes to decode
 * @vswing: set to the sink-requested voltage swing level per lane
 * @preemph: set to the sink-requested pre-emphasis level per lane
 *
 * Reads ADJUST_REQUEST_LANE0_1/2_3 (DPCD 0x206/0x207 -- contiguous, so one
 * 2-byte AUX read covers all 4 lanes).
 *
 * Return: 0 if OK, -ve on error.
 */
static int dw_dp_read_adjust_request(struct dw_dp *core, u8 lane_count,
				     u8 *vswing, u8 *preemph)
{
	int ret, lane;
	u8 adj[2];

	ret = aux_read(core, DPCD_ADJUST_REQ_LANE0_1, 2, adj);
	if (ret)
		return ret;

	for (lane = 0; lane < lane_count; lane++) {
		u8 byte = adj[lane / 2];
		bool even = !(lane % 2);

		vswing[lane] = (byte & DP_ADJUST_VOLTAGE_SWING_LANE_MASK(even)) >>
			       DP_ADJUST_VOLTAGE_SWING_LANE_SHIFT(even);
		preemph[lane] = (byte & DP_ADJUST_PRE_EMPHASIS_LANE_MASK(even)) >>
				DP_ADJUST_PRE_EMPHASIS_LANE_SHIFT(even);
	}

	return 0;
}

/**
 * dw_dp_apply_drive_settings() - apply per-lane drive settings to sink and PHY
 * @core: core state
 * @lane_count: number of lanes to program
 * @vswing: voltage swing level per lane
 * @preemph: pre-emphasis level per lane
 *
 * Applies to both the sink (TRAINING_LANEn_SET, DPCD 0x103+n) and the local
 * PHY (DPTX_PHY_TX_EQ).
 *
 * Return: 0 if OK, -ve on error.
 */
static int dw_dp_apply_drive_settings(struct dw_dp *core, u8 lane_count,
				      const u8 *vswing, const u8 *preemph)
{
	u8 lane_set[DW_DP_MAX_LANE_COUNT] = {0};
	u32 phy_tx_eq = 0;
	int lane;

	for (lane = 0; lane < lane_count; lane++) {
		lane_set[lane] = (vswing[lane] & DP_TRAIN_VOLTAGE_SWING_MASK) <<
				 DP_TRAIN_VOLTAGE_SWING_SHIFT;
		if (vswing[lane] >= DP_TRAIN_LEVEL_MAX)
			lane_set[lane] |= DP_TRAIN_MAX_SWING_REACHED;
		lane_set[lane] |= (preemph[lane] & DP_TRAIN_PRE_EMPHASIS_MASK) <<
				  DP_TRAIN_PRE_EMPHASIS_SHIFT;
		if (preemph[lane] >= DP_TRAIN_LEVEL_MAX)
			lane_set[lane] |= DP_TRAIN_MAX_PRE_EMPHASIS_REACHED;

		phy_tx_eq |= (preemph[lane] & DPTX_PHY_TX_EQ_LEVEL_MASK) <<
			     DPTX_PHY_TX_EQ_PREEMPH_SHIFT(lane);
		phy_tx_eq |= (vswing[lane] & DPTX_PHY_TX_EQ_LEVEL_MASK) <<
			     DPTX_PHY_TX_EQ_VSWING_SHIFT(lane);
	}

	dw_dp_wr(core, DPTX_PHY_TX_EQ, phy_tx_eq);

	return aux_write(core, DPCD_TRAINING_LANE0_SET, lane_count, lane_set);
}

/**
 * dw_dp_cr_done() - clock-recovery done predicate for dw_dp_train_loop()
 * @lane_status: LANE0_1_STATUS (and beyond) as read from the sink
 * @lane_count: number of lanes being trained
 *
 * NOTE: only decodes lane_status[0] (LANE0_1_STATUS), never lane_status[1]
 * (LANE2_3_STATUS) -- correct for lane_count <= 2. No caller currently
 * trains more than 2 lanes (the vendor glue driver in this tree hardcodes
 * 1-lane RBR), so this has never been exercised at lane_count == 4; extend
 * both this and dw_dp_eq_done() to also check lane_status[1] before
 * relying on 4-lane training with this core.
 *
 * Return: true if clock recovery is done on all active lanes.
 */
static bool dw_dp_cr_done(const u8 *lane_status, u8 lane_count)
{
	return (lane_status[0] & DP_LANE_CR_DONE) &&
	       (lane_count < 2 ||
		(lane_status[0] & (DP_LANE_CR_DONE << DP_LANE1_STATUS_SHIFT)));
}

static void dw_dp_cr_fail(struct dw_dp *core, const u8 *lane_status)
{
	dev_err(core->dev, "clock recovery failed, lane0_1_status=0x%02x rate=0x%x lanes=%u\n",
		lane_status[0], core->link_rate, core->lane_count);
}

/**
 * dw_dp_eq_done() - channel-equalization done predicate for dw_dp_train_loop()
 * @lane_status: LANE0_1_STATUS/LANE2_3_STATUS/LANE_ALIGN_STATUS as read from
 *		 the sink
 * @lane_count: number of lanes being trained
 *
 * NOTE: only decodes lane_status[0] (LANE0_1_STATUS), same lane_count <= 2
 * limitation as dw_dp_cr_done() -- see its comment.
 *
 * Return: true if channel equalization, symbol lock, and interlane
 *	   alignment are all done on every active lane.
 */
static bool dw_dp_eq_done(const u8 *lane_status, u8 lane_count)
{
	return (lane_status[2] & DP_INTERLANE_ALIGN_DONE) &&
	       (lane_status[0] & (DP_LANE_EQ_DONE | DP_LANE_SYMBOL_LOCKED)) ==
		(DP_LANE_EQ_DONE | DP_LANE_SYMBOL_LOCKED) &&
	       (lane_count < 2 ||
		(lane_status[0] & ((DP_LANE_EQ_DONE | DP_LANE_SYMBOL_LOCKED) <<
				    DP_LANE1_STATUS_SHIFT)) ==
		 ((DP_LANE_EQ_DONE | DP_LANE_SYMBOL_LOCKED) << DP_LANE1_STATUS_SHIFT));
}

static void dw_dp_eq_fail(struct dw_dp *core, const u8 *lane_status)
{
	dev_err(core->dev, "channel equalization failed, lane0_1=0x%02x lane2_3=0x%02x align=0x%02x\n",
		lane_status[0], lane_status[1], lane_status[2]);
}

/**
 * dw_dp_train_loop() - shared CR/EQ retry loop (DP spec 1.4 sec 3.5.1.2)
 * @core: core state
 * @status_len: LANE0_1_STATUS bytes to read (2 for CR, 3 for EQ, the third
 *		being LANE_ALIGN_STATUS)
 * @done: stage-specific done predicate (dw_dp_cr_done() or dw_dp_eq_done())
 * @fail: stage-specific failure logger (dw_dp_cr_fail() or dw_dp_eq_fail())
 * @stage: stage name, used only for logging
 * @vswing: per-lane voltage swing level, updated in place as the sink
 *	    requests changes
 * @preemph: per-lane pre-emphasis level, updated in place as the sink
 *	     requests changes
 *
 * Both CR and EQ read LANE0_1_STATUS, check a stage-specific done
 * condition, and otherwise pull the sink's adjust-request and re-apply
 * drive settings if they changed -- they differ only in @status_len and
 * the done/failure predicates.
 *
 * Return: 0 once @done reports success, -EIO if DP_TRAIN_MAX_ATTEMPTS is
 *	   exhausted, or another -ve error from a failed AUX transaction.
 */
static int dw_dp_train_loop(struct dw_dp *core, u8 status_len,
			    bool (*done)(const u8 *lane_status, u8 lane_count),
			     void (*fail)(struct dw_dp *core, const u8 *lane_status),
			     const char *stage, u8 *vswing, u8 *preemph)
{
	u8 lane_status[3] = {0};
	int i, ret;

	for (i = 0; i < DP_TRAIN_MAX_ATTEMPTS; i++) {
		u8 new_preemph[DW_DP_MAX_LANE_COUNT] = {0};
		u8 new_vswing[DW_DP_MAX_LANE_COUNT] = {0};

		mdelay(1);
		ret = aux_read(core, DPCD_LANE0_1_STATUS, status_len, lane_status);
		if (ret)
			return ret;

		if (done(lane_status, core->lane_count)) {
			dev_dbg(core->dev, "DPTX: %s done after %d attempt(s)\n",
				stage, i + 1);
			return 0;
		}

		ret = dw_dp_read_adjust_request(core, core->lane_count,
						new_vswing, new_preemph);
		if (ret)
			return ret;

		if (memcmp(new_vswing, vswing, core->lane_count) ||
		    memcmp(new_preemph, preemph, core->lane_count)) {
			memcpy(vswing, new_vswing, core->lane_count);
			memcpy(preemph, new_preemph, core->lane_count);
			ret = dw_dp_apply_drive_settings(core, core->lane_count,
							 vswing, preemph);
			if (ret)
				return ret;
		}
	}
	dev_dbg(core->dev,
		"DPTX: %s failed after %d attempts: lane_status=0x%02x%02x%02x vswing=%u/%u preemph=%u/%u\n",
		stage, DP_TRAIN_MAX_ATTEMPTS, lane_status[0], lane_status[1],
		lane_status[2], vswing[0], vswing[1], preemph[0], preemph[1]);
	fail(core, lane_status);

	return -EIO;
}

/**
 * dw_dp_train_clock_recovery() - configure and run the clock-recovery stage
 * @core: core state, with link_rate/lane_count already set
 * @vswing: per-lane voltage swing level, updated in place by the training loop
 * @preemph: per-lane pre-emphasis level, updated in place by the training loop
 *
 * Writes DOWNSPREAD_CTRL/LINK_BW_SET/LINK_COUNT_SET, sends TPS1 to sink and
 * PHY at the starting 0/0 drive level, then hands off to dw_dp_train_loop().
 *
 * Return: 0 if OK, -ve on error.
 */
static int dw_dp_train_clock_recovery(struct dw_dp *core, u8 *vswing,
				      u8 *preemph)
{
	u8 pattern_and_lanes[1 + DW_DP_MAX_LANE_COUNT] = {0};
	u8 dpcd_misc[2];
	u8 link_cfg[2];
	int ret;

	dpcd_misc[0] = DP_SPREAD_AMP_OFF;
	dpcd_misc[1] = DP_CHANNEL_CODING_8B10B;
	ret = aux_write(core, DPCD_DOWNSPREAD_CTRL, 2, dpcd_misc);
	if (ret)
		return ret;

	link_cfg[0] = core->link_rate;
	link_cfg[1] = core->lane_count |
		      (core->enhanced_frame ? DP_LANE_COUNT_ENHANCED_FRAME_EN : 0);
	ret = aux_write(core, DPCD_LINK_BW_SET, 2, link_cfg);
	if (ret)
		return ret;

	/*
	 * Start at level 0/0 (DP spec default). TRAINING_PATTERN_SET and
	 * TRAINING_LANEx_SET are bundled into one AUX burst write per
	 * DWC_dptx User Guide 8.13.3 step 7g.
	 */
	pattern_and_lanes[0] = DP_TPS1;
	pattern_and_lanes[1] = 0;
	pattern_and_lanes[2] = 0;
	ret = aux_write(core, DPCD_TRAINING_PATTERN_SET, 1 + core->lane_count,
			pattern_and_lanes);
	if (ret)
		return ret;

	ret = dw_dp_apply_drive_settings(core, core->lane_count, vswing, preemph);
	if (ret)
		return ret;

	ret = phy_set_pattern(core, DP_TPS1);
	if (ret)
		return ret;

	return dw_dp_train_loop(core, 2, dw_dp_cr_done, dw_dp_cr_fail,
				 "clock recovery", vswing, preemph);
}

/**
 * dw_dp_train_channel_eq() - configure and run the channel-equalization stage
 * @core: core state, with link_rate/lane_count/phy_rate already set
 * @vswing: per-lane voltage swing level from the clock-recovery stage,
 *	    updated in place by the training loop
 * @preemph: per-lane pre-emphasis level from the clock-recovery stage,
 *	     updated in place by the training loop
 *
 * Sends TPS2 (or TPS3 on HBR2) plus the current drive settings to sink and
 * PHY, then hands off to dw_dp_train_loop().
 *
 * Return: 0 if OK, -ve on error.
 */
static int dw_dp_train_channel_eq(struct dw_dp *core, u8 *vswing, u8 *preemph)
{
	u8 pattern_and_lanes[1 + DW_DP_MAX_LANE_COUNT] = {0};
	u8 tps2_or_higher;
	int ret;

	switch (core->phy_rate) {
	case DW_DP_PHY_RATE_HBR2:
		tps2_or_higher = DP_TPS3;	/* HBR2 */
		break;
	default:
		tps2_or_higher = DP_TPS2;	/* RBR/HBR */
		break;
	}

	/* Bundled per 8.13.3 step 7g, same as the TPS1 write above. */
	pattern_and_lanes[0] = tps2_or_higher;
	pattern_and_lanes[1] = (vswing[0] & DP_TRAIN_VOLTAGE_SWING_MASK) |
			       ((preemph[0] & DP_TRAIN_PRE_EMPHASIS_MASK) <<
				DP_TRAIN_PRE_EMPHASIS_SHIFT);
	pattern_and_lanes[2] = (vswing[1] & DP_TRAIN_VOLTAGE_SWING_MASK) |
			       ((preemph[1] & DP_TRAIN_PRE_EMPHASIS_MASK) <<
				DP_TRAIN_PRE_EMPHASIS_SHIFT);
	ret = aux_write(core, DPCD_TRAINING_PATTERN_SET, 1 + core->lane_count,
			pattern_and_lanes);
	if (ret)
		return ret;

	ret = phy_set_pattern(core, tps2_or_higher);
	if (ret)
		return ret;

	return dw_dp_train_loop(core, 3, dw_dp_eq_done, dw_dp_eq_fail,
				 "channel equalization", vswing, preemph);
}

/**
 * dw_dp_link_train() - run full DP-spec link training
 * @core: core state, with link_rate/lane_count/phy_rate already set
 *
 * Full clock-recovery/channel-equalization training per DP 1.4 sec 3.5.1.2,
 * using AUX-negotiated feedback (ADJUST_REQUEST) to converge per-lane
 * voltage swing/pre-emphasis, followed by returning the link to the
 * "no training pattern" state.
 *
 * Return: 0 if OK, -ve on error.
 */
static int dw_dp_link_train(struct dw_dp *core)
{
	u8 preemph[DW_DP_MAX_LANE_COUNT] = {0};
	u8 vswing[DW_DP_MAX_LANE_COUNT] = {0};
	u8 tps_none = DP_TPS_NONE;
	int ret;

	ret = dw_dp_train_clock_recovery(core, vswing, preemph);
	if (ret)
		return ret;

	ret = dw_dp_train_channel_eq(core, vswing, preemph);
	if (ret)
		return ret;

	ret = aux_write(core, DPCD_TRAINING_PATTERN_SET, 1, &tps_none);
	if (ret)
		return ret;

	return phy_set_pattern(core, DP_TPS_NONE);
}

/*
 * CONFIG5[13:7] init_threshold selection, DWC_dptx User Guide sec 8.18.1
 * "SST Non-DSC Single Pixel Mode Operation".
 */
#define DP_INIT_THRESHOLD_AVER_BTU_MIN		16	/* aver_btu <= this... */
#define DP_INIT_THRESHOLD_MIN			32	/* ...selects this threshold */
#define DP_INIT_THRESHOLD_HBLANK_420_MAX	40	/* h_blank <= this, 4:2:0 only... */
#define DP_INIT_THRESHOLD_420			3	/* ...selects this threshold */
#define DP_INIT_THRESHOLD_HBLANK_MAX		80	/* h_blank <= this... */
#define DP_INIT_THRESHOLD_MID			12	/* ...selects this threshold */
#define DP_INIT_THRESHOLD_DEFAULT		16	/* otherwise */

/**
 * dw_dp_configure_video() - program the video-stream timing/format registers
 * @core: core state, with link_rate/lane_count/phy_rate already set
 * @mode: the video timing to program (CONFIG1-4, MSA1-3, HBLANK_INTERVAL)
 * @pixel_clk_hz: actual measured pixel clock, used to derive MVID and the
 *		  hblank-interval link-clock count
 *
 * Derives MVID/NVID and the CONFIG5 average-bytes-per-TU/init-threshold
 * fields from the link rate, lane count, and pixel clock, then writes every
 * register (VIDEO_CONFIG1-5, MSA1-3, HBLANK_INTERVAL, VSAMPLE_CTRL) that
 * defines the stream. This does not itself enable the stream -- it just sets
 * DPTX_VSAMPLE_ENABLE as part of VSAMPLE_CTRL along with everything else.
 */
static void dw_dp_configure_video(struct dw_dp *core,
				  const struct dw_dp_mode *mode,
				  u32 pixel_clk_hz)
{
	u32 link_kbps, aver_btu, mvid, nvid, pixel_clk_khz;
	u32 init_threshold;

	/* Link symbol rate in kHz: DPCD BW code x DP_LINK_RATE_UNIT_KHZ. */
	link_kbps = (u32)core->link_rate * DP_LINK_RATE_UNIT_KHZ;

	/*
	 * MVID must be derived from the actual measured pixel clock, not a
	 * nominal request: on an asynchronous link (MISC0.SYNC_CLOCK=0) the
	 * sink reconstructs its pixel clock from MVID/NVID via its own PLL,
	 * so a mismatched value causes the sink's clock recovery to drift
	 * out of lock even though link training itself succeeds.
	 */
	pixel_clk_khz = pixel_clk_hz / 1000;
	mvid = pixel_clk_khz;
	nvid = link_kbps;

	/*
	 * Average bytes per TU for CONFIG5[6:0]:
	 *   aver_btu = (pixel_clk_kHz * bpp/8 * 64) / (link_kbps * lanes)
	 */
	aver_btu = pixel_clk_khz * DP_MSA_BYTES_PER_PIXEL_8BPC * DP_MSA_TU_SIZE /
		   (link_kbps * core->lane_count);

	/*
	 * Written unconditionally on every video-mode config; both sync
	 * polarities are fixed at 1 for this IP regardless of the mode's
	 * true polarity. DE polarity is left untouched.
	 */
	dw_dp_wr(core, DPTX_VINPUT_POLARITY_CTRL,
		 DPTX_VINPUT_POLARITY_VSYNC | DPTX_VINPUT_POLARITY_HSYNC);

	/*
	 * CONFIG1: H_ACTIVE_MASK=GENMASK(31,16), H_BLANK_MASK=GENMASK(15,2)
	 * CONFIG2: V_BLANK_MASK=GENMASK(31,16),  V_ACTIVE_MASK=GENMASK(15,0)
	 * CONFIG3: H_SYNC_WIDTH_MASK=GENMASK(30,16)
	 * CONFIG4: V_SYNC_WIDTH_MASK=GENMASK(30,16)
	 */
	dw_dp_wr(core, DPTX_VIDEO_CONFIG1,
		 ((u32)mode->h_active << DPTX_VIDEO_HI16_SHIFT) |
		 ((u32)mode->h_blank << DPTX_VIDEO_CONFIG1_HBLANK_SHIFT));
	dw_dp_wr(core, DPTX_VIDEO_CONFIG2,
		 ((u32)mode->v_blank << DPTX_VIDEO_HI16_SHIFT) | (u32)mode->v_active);
	dw_dp_wr(core, DPTX_VIDEO_CONFIG3, (u32)mode->h_sync << DPTX_VIDEO_HI16_SHIFT);
	dw_dp_wr(core, DPTX_VIDEO_CONFIG4, (u32)mode->v_sync << DPTX_VIDEO_HI16_SHIFT);

	/*
	 * CONFIG5: [6:0]=aver_bytes_per_tu, [13:7]=init_threshold,
	 * [19:16]=tu_frac (SST). init_threshold thresholds are from the
	 * DWC_dptx User Guide sec 8.18.1 "SST Non-DSC Single Pixel Mode
	 * Operation": aver_btu<=16 -> 32; h_blanking<=40 && YCbCr420 -> 3;
	 * h_blanking<=80 && !YCbCr420 -> 12; else -> 16.
	 */
	if (aver_btu <= DP_INIT_THRESHOLD_AVER_BTU_MIN)
		init_threshold = DP_INIT_THRESHOLD_MIN;
	else if ((u32)mode->h_blank <= DP_INIT_THRESHOLD_HBLANK_420_MAX)	/* 4:2:0 only */
		init_threshold = DP_INIT_THRESHOLD_420;
	else if ((u32)mode->h_blank <= DP_INIT_THRESHOLD_HBLANK_MAX)
		init_threshold = DP_INIT_THRESHOLD_MID;
	else
		init_threshold = DP_INIT_THRESHOLD_DEFAULT;

	/*
	 * Written in two steps with a 150ms gap: the reference driver
	 * requires the same delay pattern here for reliable lock.
	 */
	dw_dp_wr(core, DPTX_VIDEO_CONFIG5, aver_btu & DPTX_VIDEO_CONFIG5_AVER_BTU_MASK);
	mdelay(CONFIG5_WRITE_GAP_MS);
	dw_dp_wr(core, DPTX_VIDEO_CONFIG5,
		 (aver_btu & DPTX_VIDEO_CONFIG5_AVER_BTU_MASK) |	/* aver_bytes_per_tu */
		(init_threshold << DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_SHIFT) |
		(DPTX_VIDEO_CONFIG5_TU_FRAC_SST << DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT));

	dw_dp_wr(core, DPTX_VIDEO_MSA1,
		 ((u32)mode->h_start & DPTX_VIDEO_MSA_FIELD_MASK) |
		(((u32)mode->v_start & DPTX_VIDEO_MSA_FIELD_MASK) << DPTX_VIDEO_HI16_SHIFT));
	dw_dp_wr(core, DPTX_VIDEO_MSA2,
		 (mvid & DPTX_VIDEO_MSA23_FIELD_MASK) |
		((u32)DPTX_MSA2_BPC_8 << DPTX_MSA2_BPC_SHIFT));
	dw_dp_wr(core, DPTX_VIDEO_MSA3, nvid & DPTX_VIDEO_MSA23_FIELD_MASK);

	/*
	 * Link-clock cycles the horizontal blanking period should last,
	 * used by the hardware to pace BS/BE symbol insertion:
	 *   hblank_interval = h_blanking(px) * link_clk(kHz) / pixel_clk(kHz)
	 */
	dw_dp_wr(core, DPTX_VIDEO_HBLANK_INTERVAL,
		 ((u32)mode->h_blank * dw_dp_link_clk_khz[core->phy_rate] /
		 pixel_clk_khz) & DPTX_VIDEO_MSA_FIELD_MASK);

	/* VIDEO_MAPPING_MASK=GENMASK(20,16), value 1 = 8bpc RGB */
	dw_dp_wr(core, DPTX_VSAMPLE_CTRL,
		 DPTX_VSAMPLE_ENABLE |
		(DPTX_VSAMPLE_VIDEO_MAPPING_RGB_8BPC << DPTX_VSAMPLE_VIDEO_MAPPING_SHIFT));
}

/**
 * dw_dp_dump_video_config() - read back and log every video-config register
 * @core: core state
 *
 * Reads back every register dw_dp_configure_video() wrote and logs the
 * decoded fields. There is no source-side "sink locked video" status bit
 * on this IP, so this is the only way to confirm a write actually landed.
 */
static void dw_dp_dump_video_config(struct dw_dp *core)
{
	u32 c1, c2, c3, c4, c5, msa1, msa2, msa3, hblank, vsample;

	c1 = dw_dp_rd(core, DPTX_VIDEO_CONFIG1);
	c2 = dw_dp_rd(core, DPTX_VIDEO_CONFIG2);
	c3 = dw_dp_rd(core, DPTX_VIDEO_CONFIG3);
	c4 = dw_dp_rd(core, DPTX_VIDEO_CONFIG4);
	c5 = dw_dp_rd(core, DPTX_VIDEO_CONFIG5);
	msa1 = dw_dp_rd(core, DPTX_VIDEO_MSA1);
	msa2 = dw_dp_rd(core, DPTX_VIDEO_MSA2);
	msa3 = dw_dp_rd(core, DPTX_VIDEO_MSA3);
	hblank = dw_dp_rd(core, DPTX_VIDEO_HBLANK_INTERVAL);
	vsample = dw_dp_rd(core, DPTX_VSAMPLE_CTRL);

	dev_dbg(core->dev, "DPTX: video config: CONFIG1=0x%08x CONFIG2=0x%08x CONFIG3=0x%08x CONFIG4=0x%08x CONFIG5=0x%08x\n",
		c1, c2, c3, c4, c5);
	dev_dbg(core->dev, "DPTX: video config: MSA1=0x%08x MSA2=0x%08x MSA3=0x%08x HBLANK_INT=0x%08x VSAMPLE_CTRL=0x%08x\n",
		msa1, msa2, msa3, hblank, vsample);
	if (!(c1 >> DPTX_VIDEO_HI16_SHIFT) ||
	    !((c1 >> DPTX_VIDEO_CONFIG1_HBLANK_SHIFT) & DPTX_VIDEO_CONFIG1_HBLANK_MASK))
		dev_err(core->dev, "h_active or h_blank read back as 0 -- CONFIG1 write did not land\n");
	if (!(vsample & DPTX_VSAMPLE_ENABLE))
		dev_err(core->dev, "VIDEO_STREAM_ENABLE read back as 0 right after being set\n");
}

/**
 * dw_dp_start_crc_capture() - arm the sink's per-frame CRC test mode
 * @core: core state
 *
 * Tells the sink to start computing a per-frame CRC (DPCD TEST_SINK_START)
 * over every subsequent frame's active pixels, after checking
 * TEST_SINK_MISC.TEST_CRC_SUPPORTED since not all sinks implement this.
 */
static void dw_dp_start_crc_capture(struct dw_dp *core)
{
	u8 start = DP_TEST_SINK_START;
	u8 misc = 0;
	int ret;

	ret = aux_read(core, DPCD_TEST_SINK_MISC, 1, &misc);
	if (ret) {
		core->crc_supported = false;
		return;
	}

	core->crc_supported = !!(misc & DP_TEST_CRC_SUPPORTED);
	if (!core->crc_supported)
		return;

	ret = aux_write(core, DPCD_TEST_SINK, 1, &start);
	if (ret) {
		core->crc_supported = false;
		return;
	}

	core->crc_last_count = -1;
}

int dw_dp_core_check_crc(struct dw_dp *core)
{
	u8 r[2] = {0}, g[2] = {0}, b[2] = {0};
	u16 crc_r, crc_g, crc_b;
	int count, ret;
	u8 misc = 0;

	if (!core->crc_supported) {
		dev_dbg(core->dev, "DPTX_CRC: sink does not support TEST_CRC, skipping\n");
		return 0;
	}

	ret = aux_read(core, DPCD_TEST_SINK_MISC, 1, &misc);
	if (ret) {
		dev_err(core->dev, "DPTX_CRC: TEST_SINK_MISC read failed (%d)\n", ret);
		return ret;
	}
	count = misc & DP_TEST_CRC_COUNT_MASK;

	ret = aux_read(core, DPCD_TEST_CRC_R_CR, 2, r);
	if (!ret)
		ret = aux_read(core, DPCD_TEST_CRC_G_Y, 2, g);
	if (!ret)
		ret = aux_read(core, DPCD_TEST_CRC_B_CB, 2, b);
	if (ret) {
		dev_err(core->dev, "DPTX_CRC: CRC register read failed (%d)\n", ret);
		return ret;
	}

	crc_r = r[0] | ((u16)r[1] << DP_CRC_BYTE_HI_SHIFT);
	crc_g = g[0] | ((u16)g[1] << DP_CRC_BYTE_HI_SHIFT);
	crc_b = b[0] | ((u16)b[1] << DP_CRC_BYTE_HI_SHIFT);

	dev_dbg(core->dev, "DPTX_CRC: TEST_CRC_COUNT=%d R/Cr=0x%04x G/Y=0x%04x B/Cb=0x%04x -- %s\n",
		count, crc_r, crc_g, crc_b,
		(core->crc_last_count < 0) ?
		"first read, no baseline yet" :
		(count != core->crc_last_count) ?
		"count changed -- sink is receiving new frames" :
		"count unchanged -- sink has not processed a new frame");

	core->crc_last_count = count;

	return 0;
}

/**
 * dw_dp_recover_link() - reactive, bounded recovery for a degraded link
 * @core: core state
 *
 * Called for a link found degraded by dw_dp_core_check_link_health():
 * retrains, then, if video had already been started (core->pixel_clk_hz
 * != 0), replays dw_dp_core_start_video_stream() to restore it. U-Boot has
 * no interrupts/background tasks, so a link drop can only be caught the
 * next time something calls dw_dp_core_check_link_health() -- see its
 * recursion guard (core->recovering).
 *
 * Return: 0 if OK, -ve on error.
 */
static int dw_dp_recover_link(struct dw_dp *core)
{
	int ret;

	dev_dbg(core->dev, "DPTX: link degraded, attempting recovery (retrain)\n");

	ret = dw_dp_link_train(core);
	if (ret) {
		dev_err(core->dev, "DPTX: link recovery failed, retrain did not succeed (%d)\n",
			ret);
		return ret;
	}

	if (core->pixel_clk_hz) {
		dev_dbg(core->dev, "DPTX: link recovery: retrain succeeded, restarting video stream\n");
		ret = dw_dp_core_start_video_stream(core, &core->mode, core->pixel_clk_hz);
		if (ret)
			return ret;
	} else {
		dev_dbg(core->dev, "DPTX: link recovery: retrain succeeded, video was never started\n");
	}

	return 0;
}

/*
 * Sink self-report over AUX: DisplayPort's main link has no per-frame ACK,
 * so this asks the sink directly whether it detects a valid signal and
 * re-reads the same lane/link status used during training, to catch a
 * silent post-training link drop. Attempts recovery if the link is found
 * degraded (see dw_dp_recover_link()).
 */
int dw_dp_core_check_link_health(struct dw_dp *core)
{
	u8 sink_status = 0, pwr = 0;
	bool cr_ok, eq_ok, align_ok;
	u8 lane_status[3] = {0};
	int ret, health = 0;

	ret = aux_read(core, DPCD_SINK_STATUS, 1, &sink_status);
	if (ret) {
		dev_err(core->dev, "DPTX: SINK_STATUS AUX read failed (%d)\n", ret);
		health = ret;
	} else {
		dev_dbg(core->dev, "DPTX: SINK_STATUS=0x%x (receive_port_0=%d)\n",
			sink_status, !!(sink_status & DP_RECEIVE_PORT_0_STATUS));
	}

	ret = aux_read(core, DPCD_LANE0_1_STATUS, 3, lane_status);
	if (ret) {
		dev_err(core->dev, "DPTX: LANE0_1_STATUS AUX read failed (%d)\n", ret);
		if (!health)
			health = ret;
	} else {
		cr_ok = dw_dp_cr_done(lane_status, core->lane_count);
		eq_ok = dw_dp_eq_done(lane_status, core->lane_count);
		align_ok = !!(lane_status[2] & DP_INTERLANE_ALIGN_DONE);

		dev_dbg(core->dev, "DPTX: link status: lane0_1=0x%02x lane2_3=0x%02x align=0x%02x (cr=%d eq=%d align=%d)\n",
			lane_status[0], lane_status[1], lane_status[2],
			cr_ok, eq_ok, align_ok);
		if (!cr_ok || !eq_ok || !align_ok) {
			if (core->recovering) {
				dev_err(core->dev, "DPTX: link still degraded during recovery, not recursing\n");
				if (!health)
					health = -EBUSY;
			} else {
				core->recovering = true;
				ret = dw_dp_recover_link(core);
				core->recovering = false;
				if (!health)
					health = ret;
			}
		}
	}

	ret = aux_read(core, DPCD_SET_POWER, 1, &pwr);
	if (ret) {
		dev_err(core->dev, "DPTX: SET_POWER AUX read failed (%d)\n", ret);
		if (!health)
			health = ret;
	} else {
		dev_dbg(core->dev, "DPTX: sink SET_POWER=0x%x (%s)\n",
			pwr, (pwr == DP_SET_POWER_D0) ? "D0" : "not D0");
	}

	return health;
}

int dw_dp_core_start_video_stream(struct dw_dp *core,
				  const struct dw_dp_mode *mode,
				  ulong pixel_clk_hz)
{
	u32 ists;
	int ret;
	u8 pwr;

	/*
	 * Remember mode + pixel clock so dw_dp_recover_link() can replay
	 * this call if a reactive retrain needs to restart the stream.
	 */
	core->mode = *mode;
	core->pixel_clk_hz = pixel_clk_hz;

	/* Clear any stale FIFO fault latches before enabling the stream. */
	ists = dw_dp_rd(core, DPTX_GENERAL_INTERRUPT);
	if (ists & (DPTX_ISTS_VIDEO_FIFO_UNDERFLOW | DPTX_ISTS_VIDEO_FIFO_OVERFLOW))
		dw_dp_wr(core, DPTX_GENERAL_INTERRUPT,
			 ists & (DPTX_ISTS_VIDEO_FIFO_UNDERFLOW |
				DPTX_ISTS_VIDEO_FIFO_OVERFLOW));

	/* Re-cycle SET_POWER (D3 -> D0) before configuring video */
	pwr = DP_SET_POWER_D3;
	ret = aux_write(core, DPCD_SET_POWER, 1, &pwr);
	if (ret) {
		dev_err(core->dev, "failed to set SET_POWER=D3 before video config (%d)\n",
			ret);
		return ret;
	}
	mdelay(SET_POWER_D3_SETTLE_MS);
	pwr = DP_SET_POWER_D0;
	ret = aux_write(core, DPCD_SET_POWER, 1, &pwr);
	if (ret) {
		dev_err(core->dev, "failed to set SET_POWER=D0 before video config (%d)\n",
			ret);
		return ret;
	}
	mdelay(SET_POWER_D0_SETTLE_MS);

	dw_dp_configure_video(core, &core->mode, (u32)pixel_clk_hz);
	dw_dp_dump_video_config(core);

	/*
	 * Arms the sink's CRC test mode for later dw_dp_core_check_crc()
	 * calls -- checking it immediately here would only ever report "no
	 * baseline yet", since crc_last_count was just reset to -1.
	 */
	dw_dp_start_crc_capture(core);

	return dw_dp_core_check_link_health(core);
}

int dw_dp_core_attach(struct dw_dp *core, u8 link_rate, u8 lane_count)
{
	u8 caps[4] = {0};
	u8 sc_raw = 0;
	u32 cctl;
	int ret;
	u8 pwr;

	/* Soft reset */
	dw_dp_wr(core, DPTX_SOFT_RESET,
		 DPTX_SRST_CTRL | DPTX_SRST_PHY | DPTX_SRST_AUX);
	mdelay(ATTACH_SOFT_RESET_SETTLE_MS);
	dw_dp_wr(core, DPTX_SOFT_RESET, 0);

	ret = phy_power_sequence(core);
	if (ret)
		return ret;

	/*
	 * Clear the HW's own power-on-default fast link training before
	 * software drives its own link training sequence -- see
	 * DPTX_CCTL_DEFAULT_FAST_LINK_TRAIN_EN in dw_dp_core.h.
	 */
	cctl = dw_dp_rd(core, DPTX_CCTL) & ~DPTX_CCTL_DEFAULT_FAST_LINK_TRAIN_EN;
	dw_dp_wr(core, DPTX_CCTL, cctl);

	ret = dw_dp_wait_hpd(core);
	if (ret)
		return ret;

	/*
	 * Receiver capability read. Only MAX_LANE_COUNT bit 7
	 * (DP_ENHANCED_FRAME_CAP) is actually used, to set core->enhanced_frame
	 * for link training and the matching CCTL bit; the rest is
	 * diagnostic since rate/lanes are caller-supplied.
	 */
	ret = aux_read(core, DPCD_DPCD_REV, 4, caps);
	if (!ret) {
		core->enhanced_frame = !!(caps[2] & DP_ENHANCED_FRAME_CAP);
		dw_dp_wr(core, DPTX_CCTL,
			 core->enhanced_frame ? DPTX_CCTL_ENH_FRAME : 0);
	}

	/* Diagnostic-only: how many sink/receiver devices report behind this connection. */
	ret = aux_read(core, DPCD_SINK_COUNT, 1, &sc_raw);
	if (!ret)
		dev_dbg(core->dev, "DPTX: sink_count=%u\n",
			(u32)((sc_raw & DP_SINK_COUNT_LOW_MASK) |
			      ((sc_raw & DP_SINK_COUNT_BIT7) ? BIT(6) : 0)));

	/*
	 * Wake the sink into D0 before doing anything else: AUX and even
	 * link training can succeed while the sink's video-receive path
	 * stays powered down otherwise.
	 */
	pwr = DP_SET_POWER_D3;
	ret = aux_write(core, DPCD_SET_POWER, 1, &pwr);
	if (ret)
		return ret;
	mdelay(SINK_D3_WAKE_MS);
	pwr = DP_SET_POWER_D0;
	ret = aux_write(core, DPCD_SET_POWER, 1, &pwr);
	if (ret)
		return ret;
	mdelay(SINK_D0_SETTLE_MS);

	/* Power up all PHY lanes before link training */
	dw_dp_wr(core, DPTX_PHYIF_PWRDOWN_CTRL, 0);

	core->link_rate  = link_rate;
	core->lane_count = lane_count;
	core->phy_rate   = dpcd_bw_to_phy_rate(link_rate);

	ret = dw_dp_link_train(core);
	if (ret)
		return ret;
	dev_dbg(core->dev, "DPTX: link trained (rate=0x%x lanes=%u)\n",
		core->link_rate, core->lane_count);

	/*
	 * Not re-checking link health here: dw_dp_train_channel_eq() just
	 * polled CR/EQ/align status to success a few instructions earlier,
	 * and nothing can have changed in that window.
	 */

	/*
	 * Deliberately not enabling the video stream here: doing so before
	 * a real pixel source is feeding the video FIFO starves it
	 * immediately. Call dw_dp_core_start_video_stream() once the
	 * pixel source is ready.
	 */
	return 0;
}

/**
 * dw_dp_check_fifo_faults() - clear latched FIFO fault bits and re-check
 * @core: core state
 * @ists: GENERAL_INTERRUPT value already read by the caller
 *
 * Clears any latched VIDEO_FIFO_UNDERFLOW/OVERFLOW bits and re-checks after
 * a short delay, to distinguish a one-time historical event from an
 * actively recurring fault. Mutates hardware state (write-1-to-clear),
 * unlike the rest of dw_dp_core_debug_status()'s pure register reads.
 */
static void dw_dp_check_fifo_faults(struct dw_dp *core, u32 ists)
{
	u32 after;

	if (!(ists & (DPTX_ISTS_VIDEO_FIFO_UNDERFLOW | DPTX_ISTS_VIDEO_FIFO_OVERFLOW)))
		return;

	dw_dp_wr(core, DPTX_GENERAL_INTERRUPT,
		 ists & (DPTX_ISTS_VIDEO_FIFO_UNDERFLOW |
			DPTX_ISTS_VIDEO_FIFO_OVERFLOW));
	mdelay(FIFO_FAULT_RECHECK_MS);
	after = dw_dp_rd(core, DPTX_GENERAL_INTERRUPT);
	dev_warn(core->dev,
		 "DPTX: FIFO fault latches cleared, re-checked after recheck delay: now=0x%x (%s)\n",
		 after,
		 (after & (DPTX_ISTS_VIDEO_FIFO_UNDERFLOW |
			   DPTX_ISTS_VIDEO_FIFO_OVERFLOW)) ?
		 "still recurring" : "was a one-time event");
}

void dw_dp_core_debug_status(struct dw_dp *core)
{
	u32 hpd, phyctrl, auxstatus, ists;

	hpd = dw_dp_rd(core, DPTX_HPD_STATUS);
	phyctrl = dw_dp_rd(core, DPTX_PHYIF_CTRL);
	auxstatus = dw_dp_rd(core, DPTX_AUX_STATUS);
	ists = dw_dp_rd(core, DPTX_GENERAL_INTERRUPT);

	dev_dbg(core->dev,
		"DPTX: HPD_STATUS=0x%x (HPD_LINE=%d) PHYIF_CTRL=0x%x (PHY_BUSY=0x%x) AUX_STATUS=0x%x\n",
		hpd, !!(hpd & DPTX_HPD_LINE), phyctrl,
		(u32)((phyctrl & DPTX_PHY_BUSY_MASK) >> DPTX_PHY_BUSY_SHIFT), auxstatus);

	/* Sticky: a set bit latched at some point, not necessarily right now. */
	dev_dbg(core->dev,
		"DPTX: GENERAL_INTERRUPT=0x%x (VIDEO_FIFO_UNDERFLOW=%d VIDEO_FIFO_OVERFLOW=%d)\n",
		ists, !!(ists & DPTX_ISTS_VIDEO_FIFO_UNDERFLOW),
		!!(ists & DPTX_ISTS_VIDEO_FIFO_OVERFLOW));

	dw_dp_check_fifo_faults(core, ists);

	dw_dp_core_check_link_health(core);
	dw_dp_core_check_crc(core);
}
