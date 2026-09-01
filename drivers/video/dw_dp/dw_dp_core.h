/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Synopsys DesignWare DisplayPort TX (DPTX) core
 *
 * Generic register-level engine for the Synopsys DesignWare DPTX IP: AUX
 * channel, PHY power/pattern control, DP-spec CR/EQ link training, video
 * stream (MSA) programming, and link-health/CRC monitoring.
 *
 * This is a plain library with no driver-model registration of its own.
 * Vendor integrations (clock/reset sequencing, resource mapping,
 * driver-model registration, and board-specific rate/lane policy) live in
 * a glue driver that embeds struct dw_dp and calls the dw_dp_core_*()
 * entry points below.
 *
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 * Author: Pranav Sanwal <pranav.sanwal@amd.com>
 */

#ifndef _DW_DP_CORE_H_
#define _DW_DP_CORE_H_

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/types.h>

struct udevice;

/* Soft reset */
#define DPTX_SOFT_RESET			0x204
#define DPTX_SRST_CTRL			BIT(0)
#define DPTX_SRST_PHY			BIT(1)
#define DPTX_SRST_AUX			BIT(4)

/* PHY interface */
#define DPTX_PHYIF_CTRL			0xa00
#define DPTX_PHY_TPS(t)			((t) << 0)	/* [3:0]: 0=none,1-4=TPS */
#define DPTX_PHY_RATE(r)		((r) << 4)	/* [5:4]: 0=RBR,1=HBR,2=HBR2 */
#define DPTX_PHY_LANES(l)		((l) << 6)	/* [7:6]: lane index, not count */
#define DPTX_PHY_XMIT_EN(l)		BIT(8 + (l))	/* [11:8] per-lane transmit enable */
#define DPTX_PHY_BUSY_MASK		GENMASK(15, 12)
#define DPTX_PHY_BUSY_SHIFT		12

#define DPTX_PHYIF_PWRDOWN_CTRL	0xa18	/* 0 = power up all lanes */

/*
 * Per-lane TX equalization (voltage swing / pre-emphasis), 6 bits per lane:
 * pre-emphasis at bits [6*lane+1:6*lane], vswing at bits [6*lane+3:6*lane+2].
 * The raw DP level (0-3) is written directly, no lookup table.
 */
#define DPTX_PHY_TX_EQ			0xa04
#define DPTX_PHY_TX_EQ_PREEMPH_SHIFT(lane)	(6 * (lane))
#define DPTX_PHY_TX_EQ_VSWING_SHIFT(lane)	(6 * (lane) + 2)
#define DPTX_PHY_TX_EQ_LEVEL_MASK		0x3

/*
 * PHY_POWERDOWN, PHYIF_CTRL[20:17]. Resets to 3 (full power-down); bringing
 * the PHY up requires the 3 -> 2 -> 0 sequence below, waiting for PHY_BUSY
 * to clear at each step, before AUX or the main link will respond.
 */
#define DPTX_PHY_POWERDOWN(p)		(((u32)(p)) << 17)
#define DPTX_PHY_POWERDOWN_FULL		0x3
#define DPTX_PHY_POWERDOWN_P2		0x2
#define DPTX_PHY_POWERDOWN_ON		0x0

/* AUX channel */
#define DPTX_AUX_CMD			0xb00
#define DPTX_AUX_STATUS			0xb04
#define DPTX_AUX_DATA0			0xb08
#define DPTX_AUX_DATA1			0xb0c
#define DPTX_AUX_DATA2			0xb10
#define DPTX_AUX_DATA3			0xb14

#define DPTX_AUX_CMD_NATIVE_WRITE	(0x8u << 28)
#define DPTX_AUX_CMD_NATIVE_READ	(0x9u << 28)
/* AUX_CMD[27:8]=address, AUX_CMD[3:0]=length-1 */
#define DPTX_AUX_CMD_ADDR_SHIFT		8
#define DPTX_AUX_CMD_ADDR_MASK		0xfffff
#define DPTX_AUX_CMD_LEN_MASK		0xf
/* AUX_STATUS[16]: 1 while a transfer is active, 0 once the reply is valid. */
#define DPTX_AUX_BUSY			BIT(16)
#define DPTX_AUX_TIMEOUT		BIT(17)
#define DPTX_AUX_REPLY_ERR		BIT(18)
#define DPTX_AUX_REPLY_CODE_MASK	GENMASK(7, 4)
#define DPTX_AUX_REPLY_CODE_SHIFT	4
#define DPTX_AUX_BYTES_READ_MASK	GENMASK(23, 19)
#define DPTX_AUX_BYTES_READ_SHIFT	19
/* AUX_STATUS[27:25]: error code valid when DPTX_AUX_REPLY_ERR is set. */
#define DPTX_AUX_STATUS_ERR_CODE_SHIFT	25
#define DPTX_AUX_STATUS_ERR_CODE_MASK	0x7

#define DPTX_AUX_REPLY_ACK		0x0
#define DPTX_AUX_REPLY_NACK		0x1
#define DPTX_AUX_REPLY_DEFER		0x2

/* AUX_DATA0..3 hold up to 16 bytes (4 x 32-bit registers) per transaction. */
#define DPTX_AUX_MAX_LEN		16

/* HPD */
#define DPTX_HPD_STATUS			0xd08
#define DPTX_HPD_LINE			BIT(8)

/* General interrupt status (sticky, write-1-to-clear) */
#define DPTX_GENERAL_INTERRUPT		0xd00
#define DPTX_ISTS_VIDEO_FIFO_OVERFLOW	BIT(6)
#define DPTX_ISTS_VIDEO_FIFO_UNDERFLOW	BIT(8)

/* Link control */
#define DPTX_CCTL			0x200
#define DPTX_CCTL_ENH_FRAME		BIT(1)	/* enhanced framing mode */
/*
 * DEFAULT_FAST_LINK_TRAIN_EN, reset default 1: out of power-on reset the
 * controller autonomously starts fast link training for 1-lane RBR with
 * default 640x480 6bpc timings. Cleared once at the start of
 * dw_dp_core_attach(), before any DPCD access, so this HW-driven sequence
 * can't race software taking over training.
 */
#define DPTX_CCTL_DEFAULT_FAST_LINK_TRAIN_EN	BIT(2)

/* Video stream (stream 0) */
#define DPTX_VSAMPLE_CTRL		0x300
#define DPTX_VSAMPLE_ENABLE		BIT(5)		/* stream enable */
/* VIDEO_MAPPING_MASK = GENMASK(20,16); value 1 = 8bpc RGB */
#define DPTX_VSAMPLE_VIDEO_MAPPING_SHIFT	16
#define DPTX_VSAMPLE_VIDEO_MAPPING_RGB_8BPC	1u

#define DPTX_VINPUT_POLARITY_CTRL	0x30c
#define DPTX_VINPUT_POLARITY_VSYNC	BIT(0)
#define DPTX_VINPUT_POLARITY_HSYNC	BIT(1)

#define DPTX_VIDEO_CONFIG1		0x310	/* [31:16]=h_active, [15:2]=h_blank */
#define DPTX_VIDEO_CONFIG2		0x314	/* [31:16]=v_blank,  [15:0]=v_active */
#define DPTX_VIDEO_CONFIG3		0x318	/* [30:16]=h_sync_width */
#define DPTX_VIDEO_CONFIG4		0x31c	/* [30:16]=v_sync_width */
/* Shared by the [31:16] high-field of CONFIG1-4 (h_active/v_blank/h_sync/v_sync). */
#define DPTX_VIDEO_HI16_SHIFT		16
#define DPTX_VIDEO_CONFIG1_HBLANK_SHIFT	2	/* CONFIG1[15:2] = h_blank */
#define DPTX_VIDEO_CONFIG1_HBLANK_MASK	GENMASK(13, 0)

#define DPTX_VIDEO_CONFIG5		0x320	/* [6:0]=aver_bytes_per_tu,
						 * [13:7]=init_threshold,
						 * [19:16]=tu_frac (SST)
						 */
#define DPTX_VIDEO_CONFIG5_AVER_BTU_MASK		GENMASK(6, 0)
#define DPTX_VIDEO_CONFIG5_INIT_THRESHOLD_SHIFT	7
#define DPTX_VIDEO_CONFIG5_TU_FRAC_SHIFT		16
#define DPTX_VIDEO_CONFIG5_TU_FRAC_SST			1u

#define DPTX_VIDEO_MSA1			0x324	/* [15:0]=h_start, [31:16]=v_start */
#define DPTX_VIDEO_MSA2			0x328	/* [23:0]=mvid, [31:29]=bpc_code */
#define DPTX_VIDEO_MSA3			0x32c	/* [23:0]=nvid */
#define DPTX_VIDEO_MSA_FIELD_MASK	GENMASK(15, 0)	/* h_start/v_start/hblank_interval width */
#define DPTX_VIDEO_MSA23_FIELD_MASK	GENMASK(23, 0)	/* mvid/nvid width */
#define DPTX_VIDEO_HBLANK_INTERVAL	0x330	/* [15:0]=hblank_interval, in link-clock cycles */

#define DPTX_MSA2_BPC_8			1u	/* BPC code for MSA2[31:29] */
#define DPTX_MSA2_BPC_SHIFT		29

/* aver_bytes_per_tu formula constants, CONFIG5[6:0] (DWC_dptx UG sec 8.18.1) */
#define DP_MSA_BYTES_PER_PIXEL_8BPC	3	/* bytes/pixel at 8bpc RGB */
#define DP_MSA_TU_SIZE			64	/* transfer unit size, in symbols */

/* DPCD addresses (DP spec 1.4) */
#define DPCD_DPCD_REV			0x0
#define DP_ENHANCED_FRAME_CAP		BIT(7)

/*
 * Sink count (DP spec 1.4 Table 2-160): bits[5:0] plus bit 7 as an added
 * MSB (bit 6 is the unrelated CP_READY flag).
 */
#define DPCD_SINK_COUNT			0x200
#define DP_SINK_COUNT_LOW_MASK		GENMASK(5, 0)
#define DP_SINK_COUNT_BIT7		BIT(7)

/* DP spec supports 1, 2, or 4 lanes -- used to size per-lane arrays. */
#define DW_DP_MAX_LANE_COUNT		4

#define DPCD_LINK_BW_SET		0x100
#define DPCD_TRAINING_PATTERN_SET	0x102
/*
 * TRAINING_LANE0_SET.. (0x103 + lane), contiguous with TRAINING_PATTERN_SET.
 * DWC_dptx User Guide 8.13.3 step 7g requires these to be written in a
 * single AUX CH burst write transaction.
 */
#define DPCD_TRAINING_LANE0_SET		0x103
#define DP_TRAIN_VOLTAGE_SWING_SHIFT	0
#define DP_TRAIN_VOLTAGE_SWING_MASK	0x3
#define DP_TRAIN_MAX_SWING_REACHED	BIT(2)
#define DP_TRAIN_PRE_EMPHASIS_SHIFT	3
#define DP_TRAIN_PRE_EMPHASIS_MASK	0x3
#define DP_TRAIN_MAX_PRE_EMPHASIS_REACHED	BIT(5)
/* Highest valid DP_TRAIN_VOLTAGE_SWING_MASK / DP_TRAIN_PRE_EMPHASIS_MASK level. */
#define DP_TRAIN_LEVEL_MAX		3
/* CR/EQ training loop bound, DP 1.4 sec 3.5.1.2 recommends up to 5 attempts. */
#define DP_TRAIN_MAX_ATTEMPTS		5
#define DPCD_DOWNSPREAD_CTRL		0x107	/* + CHANNEL_CODING_SET at +1 */
#define DP_SPREAD_AMP_OFF		0x00	/* no SSC */
#define DP_CHANNEL_CODING_8B10B		0x01
#define DP_LANE_COUNT_ENHANCED_FRAME_EN	BIT(7)	/* LANE_COUNT_SET bit 7 */

/*
 * Per-lane sink feedback during CR/EQ training (DP spec 1.4 Table 2-160):
 * contiguous 2-byte read covers all 4 lanes, low nibble = even lane, high
 * nibble = odd lane.
 */
#define DPCD_ADJUST_REQ_LANE0_1	0x206
#define DP_ADJUST_VOLTAGE_SWING_LANE_MASK(even)	((even) ? 0x03 : 0x30)
#define DP_ADJUST_VOLTAGE_SWING_LANE_SHIFT(even)	((even) ? 0 : 4)
#define DP_ADJUST_PRE_EMPHASIS_LANE_MASK(even)		((even) ? 0x0c : 0xc0)
#define DP_ADJUST_PRE_EMPHASIS_LANE_SHIFT(even)	((even) ? 2 : 6)

#define DPCD_LANE0_1_STATUS		0x202
#define DPCD_SINK_STATUS		0x205
#define DPCD_SET_POWER			0x600
#define DP_SET_POWER_D0			0x1
#define DP_SET_POWER_D3			0x2
#define DP_RECEIVE_PORT_0_STATUS	BIT(0)

/* Per-frame sink CRC (DP spec 1.4 Table 2-160) */
#define DPCD_TEST_CRC_R_CR		0x240	/* 2 bytes, 16-bit CRC */
#define DPCD_TEST_CRC_G_Y		0x242	/* 2 bytes, 16-bit CRC */
#define DPCD_TEST_CRC_B_CB		0x244	/* 2 bytes, 16-bit CRC */
#define DPCD_TEST_SINK_MISC		0x246
#define DP_TEST_CRC_COUNT_MASK		GENMASK(3, 0)
#define DP_TEST_CRC_SUPPORTED		BIT(5)
#define DP_CRC_BYTE_HI_SHIFT		8	/* combine 2 AUX bytes into one u16 CRC */
#define DPCD_TEST_SINK			0x270
#define DP_TEST_SINK_START		BIT(0)

/* DPCD link bandwidth codes */
#define DP_LINK_BW_1_62		0x06
#define DP_LINK_BW_2_7		0x0a
#define DP_LINK_BW_5_4		0x14

/* PHYIF_CTRL[5:4] hardware rate encoding: 0=RBR, 1=HBR, 2=HBR2. */
#define DW_DP_PHY_RATE_RBR	0
#define DW_DP_PHY_RATE_HBR	1
#define DW_DP_PHY_RATE_HBR2	2

/* Link symbol clock (kHz) per unit of DPCD link bandwidth code. */
#define DP_LINK_RATE_UNIT_KHZ	27000

/* Training pattern values */
#define DP_TPS_NONE		0
#define DP_TPS1			1
#define DP_TPS2			2
#define DP_TPS3			3

/*
 * Lane status masks (DPCD LANE0_1_STATUS, 0x202); lane 1's triplet is the
 * same 3 bits shifted up by 4.
 */
#define DP_LANE_CR_DONE		BIT(0)
#define DP_LANE_EQ_DONE		BIT(1)
#define DP_LANE_SYMBOL_LOCKED	BIT(2)
#define DP_LANE1_STATUS_SHIFT	4	/* lane 1's triplet, shifted up by 4 */
#define DP_INTERLANE_ALIGN_DONE	BIT(0)

/**
 * struct dw_dp_mode - video mode/timing parameters for the video stream
 *
 * @h_active: horizontal active pixels (CONFIG1[31:16])
 * @h_blank: total horizontal blanking: front porch + sync + back porch
 *	(CONFIG1[15:2])
 * @h_sync: horizontal sync pulse width (CONFIG3[30:16])
 * @h_start: offset from the leading edge of hsync to the start of active
 *	video, i.e. sync + back porch (MSA1[15:0])
 * @v_active: vertical active lines (CONFIG2[15:0])
 * @v_blank: total vertical blanking: front porch + sync + back porch
 *	(CONFIG2[31:16])
 * @v_sync: vertical sync pulse width (CONFIG4[30:16])
 * @v_start: offset from the leading edge of vsync to the start of active
 *	video, i.e. sync + back porch (MSA1[31:16])
 *
 * Register-level counterpart to the generic struct display_timing a
 * UCLASS_VIDEO consumer builds from whatever timing source it has. The
 * vendor glue driver's struct dm_display_ops enable_stream op translates
 * from struct display_timing to this struct (recombining front/back
 * porch into h_start/h_blank etc., since that's what this core's MSA
 * registers expect) before calling dw_dp_core_start_video_stream().
 * Always programmed as 8bpc RGB (DPTX_MSA2_BPC_8/
 * DPTX_VSAMPLE_VIDEO_MAPPING_RGB_8BPC) -- there is no bpc field here since
 * dw_dp_configure_video() does not support anything else yet.
 */
struct dw_dp_mode {
	u16 h_active;
	u16 h_blank;
	u16 h_sync;
	u16 h_start;
	u16 v_active;
	u16 v_blank;
	u16 v_sync;
	u16 v_start;
};

/**
 * struct dw_dp - Synopsys DesignWare DPTX core state
 *
 * @dev: owning udevice, kept for dev_err()/log context only -- the core
 *	itself is never bound to a uclass or driver-model instance.
 * @base: mapped base address of the DPTX register block.
 * @lane_count: number of lanes currently trained (1, 2, or 4).
 * @link_rate: DPCD link bandwidth code currently trained, e.g.
 *	DP_LINK_BW_1_62.
 * @phy_rate: hardware PHYIF_CTRL rate field derived from @link_rate:
 *	0=RBR, 1=HBR, 2=HBR2.
 * @enhanced_frame: DP_ENHANCED_FRAME_CAP, from DPCD_DPCD_REV byte 2 bit 7.
 * @crc_supported: whether the sink advertised TEST_CRC support at the
 *	last dw_dp_core_start_video_stream() call.
 * @crc_last_count: last TEST_CRC_COUNT seen by dw_dp_core_check_crc(), or
 *	-1 if no reading has been taken yet.
 * @mode: the last mode passed to dw_dp_core_start_video_stream(),
 *	retained so a reactive link recovery can replay it.
 * @pixel_clk_hz: the last pixel clock passed to
 *	dw_dp_core_start_video_stream(); zero means video was never started.
 * @recovering: recursion guard, since restarting the video stream after a
 *	recovery checks link health again.
 *
 * Vendor glue drivers embed this struct in their own priv struct and pass
 * &priv->core to every dw_dp_core_*() entry point.
 */
struct dw_dp {
	struct udevice *dev;
	void __iomem	*base;

	u8		lane_count;
	u8		link_rate;
	u8		phy_rate;
	bool		enhanced_frame;

	bool		crc_supported;
	int		crc_last_count;

	struct dw_dp_mode mode;
	ulong		pixel_clk_hz;

	bool		recovering;
};

static inline u32 dw_dp_rd(struct dw_dp *core, u32 reg)
{
	return readl(core->base + reg);
}

static inline void dw_dp_wr(struct dw_dp *core, u32 reg, u32 val)
{
	writel(val, core->base + reg);
}

/**
 * dw_dp_link_rate_name() - human-readable name for a DPCD link-rate code
 * @link_rate: DPCD link bandwidth code (e.g. DP_LINK_BW_1_62)
 *
 * Return: "RBR"/"HBR"/"HBR2" for a recognized code, "unknown" otherwise.
 */
const char *dw_dp_link_rate_name(u8 link_rate);

/**
 * dw_dp_core_attach() - bring up the DPTX core and run link training
 * @core: core state (base/dev must already be set)
 * @link_rate: desired DPCD link bandwidth code (e.g. DP_LINK_BW_1_62)
 * @lane_count: desired lane count (1, 2, or 4)
 *
 * Soft-resets the controller, runs the PHY power sequence, waits for
 * HPD, reads sink capability/sink-count diagnostics, wakes the sink to D0,
 * powers up the PHY lanes, then runs full DP-spec CR/EQ link training at
 * the caller-supplied rate/lane count. Does NOT enable the video stream --
 * call dw_dp_core_start_video_stream() once a real pixel source is ready.
 *
 * Return: 0 if OK, -ve on error.
 */
int dw_dp_core_attach(struct dw_dp *core, u8 link_rate, u8 lane_count);

/**
 * dw_dp_core_start_video_stream() - program video timing/MSA and enable the
 *				      video stream
 * @core: core state, already attached (dw_dp_core_attach() succeeded)
 * @mode: video timing parameters
 * @pixel_clk_hz: the actual measured pixel clock rate. On an asynchronous
 *		  link (MISC0.SYNC_CLOCK=0) the sink reconstructs its pixel
 *		  clock from MVID/NVID, so this must be the real rate, not a
 *		  nominal request.
 *
 * Return: 0 if OK, -ve on error.
 */
int dw_dp_core_start_video_stream(struct dw_dp *core,
				  const struct dw_dp_mode *mode,
				  ulong pixel_clk_hz);

/**
 * dw_dp_core_check_link_health() - re-check sink/lane status over AUX and
 *				     self-heal if degraded
 * @core: core state
 *
 * Return: 0 if the link is healthy (or was found degraded and recovery
 *	   succeeded), -ve if an AUX read failed or recovery did not succeed.
 */
int dw_dp_core_check_link_health(struct dw_dp *core);

/**
 * dw_dp_core_check_crc() - read and log the sink's per-frame CRC state
 * @core: core state
 *
 * Return: 0 if OK (including the sink not supporting TEST_CRC, a no-op
 *	   rather than a failure), -ve if an AUX read failed.
 */
int dw_dp_core_check_crc(struct dw_dp *core);

/**
 * dw_dp_core_debug_status() - dump HPD/PHY/AUX/interrupt status and run a
 *				link-health/CRC check, for on-demand diagnostics
 * @core: core state
 */
void dw_dp_core_debug_status(struct dw_dp *core);

#endif /* _DW_DP_CORE_H_ */
