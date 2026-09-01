// SPDX-License-Identifier: GPL-2.0
/*
 * AMD MMI Display Controller (DC) video driver
 *
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Pranav Sanwal <pranav.sanwal@amd.com>
 */

#define LOG_CATEGORY	UCLASS_VIDEO

#include <clk.h>
#include <display.h>
#include <dm.h>
#include <dma.h>
#include <errno.h>
#include <reset.h>
#include <time.h>
#include <video.h>
#include <video_dma.h>

#include <dm/device_compat.h>
#include <dm/ofnode.h>
#include <dm/ofnode_graph.h>
#include <dm/uclass.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>

#include "amd_mmi_dc.h"

#define DC_STC_CLK_RATE_HZ		27000000UL
#define DC_AVBUF_SRST_DELAY_MS		50	/* AVBUF soft-reset settle time */
#define DC_VSYNC_WAIT_TIMEOUT_MS	34	/* ~2 frames at 640x480@60 */
#define DC_VIDEO_STREAM_SETTLE_MS	50	/* ~3 frames at 640x480@60 */
#define DC_LINK_SETTLE_CHECK_MS	200	/* let link settle before final health/CRC check */
#define DC_VSYNC_POLL_US		50	/* wait_vsync_edge() poll interval */

/**
 * dc_set_and_enable_clk() - look up, optionally set the rate of, and enable
 *			      one DC clock
 * @dev: the amd_mmi_dc device
 * @name: clock-names entry to look up (e.g. "mmi_pll", "ps_vid_clk")
 * @rate_hz: rate to request via clk_set_rate(), or 0 to just enable the
 *	     clock at whatever rate it's already running
 * @out_hz: if non-NULL, set to the actual rate clk_get_rate() reports after
 *	    enabling
 * @optional: true if @name is allowed to be absent from the DT's
 *	      clock-names -- clk_get_by_name() failing with -ENODATA or
 *	      -ENOENT then means "not described", logged quietly, not as an
 *	      error. Any other failure (bad phandle, provider not probed
 *	      yet, etc.) still logs loudly regardless of @optional, since
 *	      that means the clock IS described but something is wrong.
 *
 * Return: 0 if OK, -ve on error.
 */
static int dc_set_and_enable_clk(struct udevice *dev, const char *name,
				 ulong rate_hz, ulong *out_hz, bool optional)
{
	struct clk clk;
	ulong got_hz;
	int ret;

	ret = clk_get_by_name(dev, name, &clk);
	if (ret) {
		if (optional && (ret == -ENODATA || ret == -ENOENT))
			dev_dbg(dev, "%s not described in DT, skipping (optional)\n",
				name);
		else
			dev_err(dev, "clk_get_by_name(%s) failed (%d), skipping (%s) -- check clock-names in the DT\n",
				name, ret, optional ? "optional" : "required");
		return ret;
	}

	if (rate_hz) {
		ret = clk_set_rate(&clk, rate_hz);
		if (ret < 0) {
			dev_err(dev, "clk_set_rate(%s, %lu) failed (%d)\n", name,
				rate_hz, ret);
			return ret;
		}
	}

	ret = clk_enable(&clk);
	if (ret) {
		dev_err(dev, "clk_enable(%s) failed (%d)\n", name, ret);
		return ret;
	}

	got_hz = clk_get_rate(&clk);
	dev_dbg(dev, "%s enabled, clk_get_rate() reports %lu Hz\n", name, got_hz);

	if (out_hz)
		*out_hz = got_hz;

	return 0;
}

static int dc_enable_clocks(struct udevice *dev)
{
	struct amd_mmi_dc_priv *priv = dev_get_priv(dev);
	int ret;

	/*
	 * mmi_pll and stc_ref_clk are optional, matching the Linux mmi_dc
	 * driver's treatment of both -- absent or failed is logged (inside
	 * dc_set_and_enable_clk()) and skipped, not a probe failure.
	 */
	dc_set_and_enable_clk(dev, "mmi_pll", 0, NULL, true);
	dc_set_and_enable_clk(dev, "stc_ref_clk", DC_STC_CLK_RATE_HZ, NULL, true);

	ret = dc_set_and_enable_clk(dev, "ps_vid_clk", DC_PIXEL_CLK_HZ,
				    &priv->pixel_clk_hz, false);
	if (ret)
		return ret;

	if (!priv->pixel_clk_hz) {
		dev_warn(dev, "ps_vid_clk enabled but clk_get_rate() reported 0 Hz, assuming nominal %lu Hz\n",
			 (ulong)DC_PIXEL_CLK_HZ);
		priv->pixel_clk_hz = DC_PIXEL_CLK_HZ;
	}

	return 0;
}

static void dc_init(struct amd_mmi_dc_priv *priv)
{
	/*
	 * Row-major 3x3 input CSC. AVBUF format code 32 unpacks the framebuffer
	 * word the same way the Linux mmi_dc driver does for its code-32 format,
	 * which that driver always pairs with an R<->B channel swap. U-Boot's
	 * VIDEO_RGBA8888 layout relies on that swap too, so map output R from
	 * input B and output B from input R (rows 0 and 2 swapped) instead of a
	 * plain identity, otherwise the channels are misordered and black (whose
	 * only set byte is the forced 0xff) renders as red.
	 */
	static const u32 csc_rb_swap[9] = {
		DC_BLEND_CSC_IDENTITY_OFF, DC_BLEND_CSC_IDENTITY_OFF, DC_BLEND_CSC_IDENTITY_DIAG,
		DC_BLEND_CSC_IDENTITY_OFF, DC_BLEND_CSC_IDENTITY_DIAG, DC_BLEND_CSC_IDENTITY_OFF,
		DC_BLEND_CSC_IDENTITY_DIAG, DC_BLEND_CSC_IDENTITY_OFF, DC_BLEND_CSC_IDENTITY_OFF,
	};
	int i;

	/* Disable write protection */
	DC_MISC_WR(priv, DC_MISC_WPROTS, 0);

	/* Enable frame switching for all layers */
	DC_MISC_WR(priv, DC_MISC_FRAME_SWITCH, DC_MISC_FRAME_SW_ALL);

	/* Use PS pixel clock */
	DC_MISC_WR(priv, DC_MISC_VID_CLK, DC_MISC_VID_CLK_PS);

	/* AVBUF: select internal timing source */
	DC_AVBUF_WR(priv, DC_AVBUF_CLK_SOURCE, DC_AVBUF_CLK_INTERNAL);

	/*
	 * A soft reset is required after changing the clock source.
	 * AVBUF_FORMAT must be programmed AFTER this reset, not before --
	 * the reset otherwise wipes out a format written beforehand.
	 */
	DC_AVBUF_WR(priv, DC_AVBUF_SRST, DC_AVBUF_SRST_ASSERT);
	mdelay(DC_AVBUF_SRST_DELAY_MS);
	DC_AVBUF_WR(priv, DC_AVBUF_SRST, 0);

	/* AVBUF: pixel format for DCDMA video plane 0 (RGBA8888 = code 32) */
	DC_AVBUF_WR(priv, DC_AVBUF_FORMAT, DC_AVBUF_FMT_RGBA8888);

	/* Functional mode (not bypass) */
	DC_MISC_WR(priv, DC_MISC_BYPASS, 0);

	/* Non-live latency */
	DC_AVBUF_WR(priv, DC_AVBUF_NON_LIVE_LATENCY,
		    DC_AVBUF_NON_LIVE_LATENCY_VAL);

	/* Blender: black background */
	DC_BLEND_WR(priv, DC_BLEND_BG_CLR(0), 0);
	DC_BLEND_WR(priv, DC_BLEND_BG_CLR(1), 0);
	DC_BLEND_WR(priv, DC_BLEND_BG_CLR(2), 0);

	/* Blender: alpha disabled, RGB output */
	DC_BLEND_WR(priv, DC_BLEND_GLOBAL_ALPHA, 0);
	DC_BLEND_WR(priv, DC_BLEND_OUTPUT_FMT, 0);

	/* Blender: R<->B swap CSC to match the AVBUF RGBA8888 unpack order */
	for (i = 0; i < ARRAY_SIZE(csc_rb_swap); i++)
		DC_BLEND_WR(priv, DC_BLEND_CSC_COEFF(i), csc_rb_swap[i]);
	for (i = 0; i < DC_BLEND_NUM_RGB_CHANNELS; i++)
		DC_BLEND_WR(priv, DC_BLEND_OUTCSC_OFF(i), 0);
}

/**
 * dc_enable_plane0() - enable the layer-0 (video plane 0 / DCDMA channel
 *			 "vid.0.0") datapath
 * @priv: the amd_mmi_dc private data
 *
 * Brings up the path so pixels fed by DCDMA actually reach the blender
 * output. Order matches the Linux mmi_dc_compositor_enable() +
 * mmi_dc_avbuf_dma_enable() sequence: layer control (blend) -> pixel
 * source select (avbuf) -> channel buffer enable (avbuf). Per-layer CSC
 * coefficients/scale-factors are left at their hardware reset defaults,
 * which already encode an identity 8bpc RGB passthrough.
 */
static void dc_enable_plane0(struct amd_mmi_dc_priv *priv)
{
	u32 val;

	/* Layer 0 input is RGB, not YUV; no chroma upsampling needed */
	DC_BLEND_WR(priv, DC_BLEND_LAYER0_CTRL, DC_BLEND_LAYER_RGB_MODE);

	/* Select "video from memory" as layer 0's pixel source */
	val = DC_AVBUF_RD(priv, DC_AVBUF_OUT_AV_SEL);
	val &= ~DC_AVBUF_OUT_AV_SEL_MASK(0);
	val |= DC_AVBUF_OUT_AV_SEL_MEM(0);
	DC_AVBUF_WR(priv, DC_AVBUF_OUT_AV_SEL, val);

	DC_AVBUF_WR(priv, DC_AVBUF_CHBUF0, DC_AVBUF_CHBUF_FLUSH);
	mdelay(1);

	/* Let the DCDMA channel 0 AXI stream into the pixel FIFO */
	DC_AVBUF_WR(priv, DC_AVBUF_CHBUF0,
		    DC_AVBUF_CHBUF_EN | DC_AVBUF_CHBUF_BURST_MAX);

	dev_dbg(priv->dev, "DC: plane0 datapath enabled (layer ctrl, avbuf select=MEM, chbuf0 en)\n");
}

/**
 * dc_set_timing() - program the DP-stream timing generator
 * @priv: the amd_mmi_dc private data
 *
 * Writes this board's fixed 640x480@60Hz timing (the DC_H_/DC_V_ constants)
 * and 8-bpc RGBA8888 pixel format into the DP-stream region.
 */
static void dc_set_timing(struct amd_mmi_dc_priv *priv)
{
	DC_DP_WR(priv, DC_DP_HTOTAL,  DC_H_TOTAL);
	DC_DP_WR(priv, DC_DP_VTOTAL,  DC_V_TOTAL);
	DC_DP_WR(priv, DC_DP_HSWIDTH, DC_H_SYNC);
	DC_DP_WR(priv, DC_DP_VSWIDTH, DC_V_SYNC);
	DC_DP_WR(priv, DC_DP_HRES,    DC_H_ACTIVE);
	DC_DP_WR(priv, DC_DP_VRES,    DC_V_ACTIVE);
	DC_DP_WR(priv, DC_DP_HSTART,  DC_H_START);
	DC_DP_WR(priv, DC_DP_VSTART,  DC_V_START);
	/* 8-bpc per component; RGBA8888 framebuffer */
	DC_DP_WR(priv, DC_DP_MISC0,   DC_DP_BPC_8);

	dev_dbg(priv->dev, "DC: timing programmed (%ux%u, htotal=%u vtotal=%u)\n",
		AMD_MMI_DC_WIDTH, AMD_MMI_DC_HEIGHT, DC_H_TOTAL, DC_V_TOTAL);
}

/**
 * dc_wait_vsync_edge() - block until a fresh VSYNC edge is seen
 * @ctx: the amd_mmi_dc private data, cast from struct video_dma_meta's
 *	 opaque wait_vsync callback context
 *
 * DCDMA's first descriptor fetch only happens once the trigger bit is set
 * AND a VSYNC edge occurs; a trigger written at an arbitrary phase can
 * land outside the blanking window and auto-pause the channel. Linux
 * dodges this with a short, precise pre-trigger delay
 * (mmi_dcdma_chan_start_transfer()).
 *
 * Return: 0 once a fresh edge is seen (or if there is no "irq" reg region
 *	   to poll at all), -ETIMEDOUT if none arrives within
 *	   DC_VSYNC_WAIT_TIMEOUT_MS.
 */
static int dc_wait_vsync_edge(void *ctx)
{
	struct amd_mmi_dc_priv *priv = ctx;
	ulong start, elapsed;
	u32 status;

	if (!priv->irq_base)
		return 0;

	/* Clear the sticky bit so the next read only reflects a fresh edge */
	DC_IRQ_WR(priv, DC_IRQ_DP_INT_STATUS, DC_IRQ_DP_INT_VSYNC_TS);

	start = get_timer(0);
	do {
		elapsed = get_timer(start);
		status = DC_IRQ_RD(priv, DC_IRQ_DP_INT_STATUS);
		if (status & DC_IRQ_DP_INT_VSYNC_TS) {
			dev_dbg(priv->dev, "DC: wait_vsync: fresh VSYNC edge seen after %lu ms\n",
				elapsed);
			return 0;
		}
		udelay(DC_VSYNC_POLL_US);
	} while (elapsed < DC_VSYNC_WAIT_TIMEOUT_MS);

	dev_err(priv->dev, "DC: wait_vsync: TIMEOUT after %u ms, DP_INT_STATUS=0x%x -- %s\n",
		DC_VSYNC_WAIT_TIMEOUT_MS, status,
		"timing generator not pulsing? (see VSYNC_TS bit)");

	return -ETIMEDOUT;
}

/**
 * dc_dump_state() - log every register this driver has written so far
 * @priv: the amd_mmi_dc private data
 * @when: short description of the checkpoint this dump is taken at,
 *	  included in each logged line
 *
 * Confirms whether writes actually latched (vs. e.g. being silently
 * dropped by write-protection re-asserting, or a shadow/double-buffer
 * register that never got committed).
 */
static void dc_dump_state(struct amd_mmi_dc_priv *priv, const char *when)
{
	dev_dbg(priv->dev, "DC: register dump (%s):\n", when);
	dev_dbg(priv->dev, "  DP_HTOTAL=%u VTOTAL=%u HRES=%u VRES=%u HSTART=%u VSTART=%u MISC0=0x%x\n",
		DC_DP_RD(priv, DC_DP_HTOTAL), DC_DP_RD(priv, DC_DP_VTOTAL),
		DC_DP_RD(priv, DC_DP_HRES), DC_DP_RD(priv, DC_DP_VRES),
		DC_DP_RD(priv, DC_DP_HSTART), DC_DP_RD(priv, DC_DP_VSTART),
		DC_DP_RD(priv, DC_DP_MISC0));
	dev_dbg(priv->dev, "  AVBUF: FORMAT=0x%x CLK_SOURCE=0x%x OUT_AV_SEL=0x%x CHBUF0=0x%x\n",
		DC_AVBUF_RD(priv, DC_AVBUF_FORMAT),
		DC_AVBUF_RD(priv, DC_AVBUF_CLK_SOURCE),
		DC_AVBUF_RD(priv, DC_AVBUF_OUT_AV_SEL),
		DC_AVBUF_RD(priv, DC_AVBUF_CHBUF0));
	dev_dbg(priv->dev, "  MISC: BYPASS=0x%x VID_CLK=0x%x FRAME_SWITCH=0x%x WPROTS=0x%x\n",
		DC_MISC_RD(priv, DC_MISC_BYPASS),
		DC_MISC_RD(priv, DC_MISC_VID_CLK),
		DC_MISC_RD(priv, DC_MISC_FRAME_SWITCH),
		DC_MISC_RD(priv, DC_MISC_WPROTS));
	dev_dbg(priv->dev, "  BLEND: LAYER0_CTRL=0x%x OUTPUT_FMT=0x%x\n",
		DC_BLEND_RD(priv, DC_BLEND_LAYER0_CTRL),
		DC_BLEND_RD(priv, DC_BLEND_OUTPUT_FMT));

	if (priv->irq_base) {
		u32 dp_int = DC_IRQ_RD(priv, DC_IRQ_DP_INT_STATUS);

		dev_dbg(priv->dev, "  IRQ: DP_INT_STATUS=0x%x (VSYNC=%d UNDERFLW=%d OVERFLW=%d FMT_ERR=%d)\n",
			dp_int, !!(dp_int & DC_IRQ_DP_INT_VSYNC_TS),
			!!(dp_int & DC_IRQ_DP_INT_CHBUF0_UNDERFLW),
			!!(dp_int & DC_IRQ_DP_INT_CHBUF0_OVERFLW),
			!!(dp_int & DC_IRQ_DP_INT_VIDTILE_FMT_ERR));
		if (!(dp_int & DC_IRQ_DP_INT_VSYNC_TS))
			dev_dbg(priv->dev, "  IRQ: VSYNC_TS=0 -- timing generator not pulsing yet\n");
	}
}

static int amd_mmi_dc_of_to_plat(struct udevice *dev)
{
	struct amd_mmi_dc_priv *priv = dev_get_priv(dev);
	struct resource res;
	int ret;

	priv->dev = dev;

	ret = dev_read_resource_byname(dev, "dp", &res);
	if (ret)
		return ret;
	priv->dp_base = ioremap(res.start, resource_size(&res));
	if (!priv->dp_base)
		return -ENOMEM;

	ret = dev_read_resource_byname(dev, "blend", &res);
	if (ret)
		return ret;
	priv->blend_base = ioremap(res.start, resource_size(&res));
	if (!priv->blend_base)
		return -ENOMEM;

	ret = dev_read_resource_byname(dev, "avbuf", &res);
	if (ret)
		return ret;
	priv->avbuf_base = ioremap(res.start, resource_size(&res));
	if (!priv->avbuf_base)
		return -ENOMEM;

	ret = dev_read_resource_byname(dev, "misc", &res);
	if (ret)
		return ret;
	priv->misc_base = ioremap(res.start, resource_size(&res));
	if (!priv->misc_base)
		return -ENOMEM;

	ret = dev_read_resource_byname(dev, "irq", &res);
	if (!ret)
		priv->irq_base = ioremap(res.start, resource_size(&res));
	else
		dev_dbg(dev, "no \"irq\" reg region in DT, VSYNC/underflow diagnostics unavailable\n");

	return 0;
}

/**
 * dc_bringup_hw() - bring up the DC hardware: reset, clocks, timing, plane0
 * @dev: the amd_mmi_dc device
 * @priv: the amd_mmi_dc private data
 *
 * Return: 0 if OK, -ve on error.
 */
static int dc_bringup_hw(struct udevice *dev, struct amd_mmi_dc_priv *priv)
{
	struct reset_ctl_bulk rst;
	bool have_reset = false;
	int ret;

	ret = reset_get_bulk(dev, &rst);
	if (!ret) {
		have_reset = true;
		reset_assert_bulk(&rst);
		dev_dbg(dev, "DC: reset asserted, held during configuration (released later, see below)\n");
	} else if (ret == -ENODATA || ret == -ENOENT) {
		dev_dbg(dev, "DC: no reset in DT, skipping\n");
	} else {
		dev_err(dev, "failed to get reset: %d\n", ret);
		return ret;
	}

	dc_init(priv);
	dev_dbg(dev, "DC: hardware init done (misc/avbuf/blend global setup)\n");

	/*
	 * Must happen before dc_set_timing(): the timing generator's counter
	 * is driven by this clock, so it never advances (and DC never pulls
	 * frames from DCDMA) unless it is actually running.
	 */
	ret = dc_enable_clocks(dev);
	if (ret) {
		dev_err(dev, "failed to enable ps_vid_clk: %d\n", ret);
		if (have_reset)
			reset_deassert_bulk(&rst);
		return ret;
	}

	dc_set_timing(priv);
	dc_enable_plane0(priv);

	/* Release reset now that all configuration is complete. */
	if (have_reset) {
		reset_deassert_bulk(&rst);
		dev_dbg(dev, "DC: reset released (all configuration complete)\n");
	}

	return 0;
}

/**
 * dc_start_dcdma() - acquire the DCDMA channel and start the cyclic
 *		       pixel transfer
 * @dev: the amd_mmi_dc device
 * @priv: the amd_mmi_dc private data
 * @uc_priv: the video uclass private data (resolution, line length)
 * @uc_plat: the video uclass platform data (framebuffer base address)
 *
 * Return: 0 if OK, -ve on error.
 */
static int dc_start_dcdma(struct udevice *dev, struct amd_mmi_dc_priv *priv,
			  struct video_priv *uc_priv,
			  struct video_uc_plat *uc_plat)
{
	struct video_dma_meta dma_meta;
	int ret;

	ret = dma_get_by_name(dev, "vid.0.0", &priv->dma);
	if (ret) {
		dev_err(dev, "DCDMA channel not found: %d\n", ret);
		return ret;
	}
	dev_dbg(dev, "DC: DCDMA channel vid.0.0 acquired (id=%lu)\n", priv->dma.id);

	dma_meta.line_size = uc_priv->line_length;
	dma_meta.stride    = uc_priv->line_length;
	dma_meta.mode      = VIDEO_DMA_CYCLIC;
	dma_meta.wait_vsync = priv->irq_base ? dc_wait_vsync_edge : NULL;
	dma_meta.vsync_ctx  = priv;
	if (priv->irq_base)
		dev_dbg(dev, "DC: DCDMA trigger will be VSYNC-gated (wait up to %u ms/attempt)\n",
			DC_VSYNC_WAIT_TIMEOUT_MS);
	else
		dev_dbg(dev, "DC: no \"irq\" reg region, DCDMA trigger will not be VSYNC-gated\n");

	dev_dbg(dev, "DC: starting DCDMA cyclic transfer fb=0x%lx size=%u\n",
		(ulong)uc_plat->base, uc_priv->fb_size);
	ret = dma_send(&priv->dma, (void *)uc_plat->base,
		       uc_priv->fb_size, &dma_meta);
	if (ret) {
		dev_err(dev, "DCDMA transfer failed: %d\n", ret);
		dma_free(&priv->dma);
		return ret;
	}

	return 0;
}

/**
 * dc_report_video_stream() - enable the video stream once DCDMA is feeding
 *			      pixels, then report the final link status
 * @dptx: the amd_mmi_dptx device
 * @priv: the amd_mmi_dc private data
 *
 * Return: 0 if OK, -ve on error.
 */
static int dc_report_video_stream(struct udevice *dptx,
				  struct amd_mmi_dc_priv *priv)
{
	struct display_timing timing = {
		.pixelclock   = { .typ = priv->pixel_clk_hz },
		.hactive      = { .typ = DC_H_ACTIVE },
		.hfront_porch = { .typ = DC_H_BLANK - DC_H_START },
		.hback_porch  = { .typ = DC_H_START - DC_H_SYNC },
		.hsync_len    = { .typ = DC_H_SYNC },
		.vactive      = { .typ = DC_V_ACTIVE },
		.vfront_porch = { .typ = DC_V_BLANK - DC_V_START },
		.vback_porch  = { .typ = DC_V_START - DC_V_SYNC },
		.vsync_len    = { .typ = DC_V_SYNC },
	};
	const char *rate_name;
	u32 link_rate_mbps;
	u8 lane_count;
	int ret;

	mdelay(DC_VIDEO_STREAM_SETTLE_MS);
	ret = display_enable_stream(dptx, &timing);
	if (ret)
		return ret;

	/*
	 * If VSYNC_TS is still 0 here, the timing generator has not produced
	 * a single VSYNC pulse in the whole window dma_send() spent polling
	 * for a validated descriptor -- distinguishes "DC never starts" from
	 * "DC starts, just later than DCDMA's timeout".
	 */
	dc_dump_state(priv, "after DCDMA kick-off attempt, probe end");

	mdelay(DC_LINK_SETTLE_CHECK_MS);
	ret = display_check_link_health(dptx);
	if (ret)
		return ret;

	ret = display_check_crc(dptx);
	if (ret)
		return ret;

	ret = display_get_link_params(dptx, &link_rate_mbps, &lane_count, &rate_name);
	if (ret)
		return ret;

	dev_info(priv->dev, "DC: display active - %ux%u framebuffer, DP link %u.%03u Gbps (%s) x%u lanes\n",
		 AMD_MMI_DC_WIDTH, AMD_MMI_DC_HEIGHT,
		 link_rate_mbps / 1000, link_rate_mbps % 1000, rate_name, lane_count);

	return 0;
}

static int amd_mmi_dc_probe(struct udevice *dev)
{
	struct video_uc_plat *uc_plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	struct amd_mmi_dc_priv *priv = dev_get_priv(dev);
	struct udevice *dptx;
	ofnode dptx_node;
	int ret;

	dev_dbg(dev, "DC: probe start\n");

	/* mmi_dc's own "ports/port@c/endpoint" DT graph link to mmi_dptx */
	dptx_node = ofnode_graph_get_remote_node(dev_ofnode(dev),
						 AMD_MMI_DC_DPTX_PORT, 0);
	if (!ofnode_valid(dptx_node)) {
		dev_err(dev, "DPTX device not found via port graph\n");
		return -ENODEV;
	}

	ret = uclass_get_device_by_ofnode(UCLASS_DISPLAY, dptx_node, &dptx);
	if (ret) {
		dev_err(dev, "DPTX device not found: %d\n", ret);
		return ret;
	}
	dev_dbg(dev, "DC: DPTX device found (%s)\n", dptx->name);

	uc_priv->xsize       = AMD_MMI_DC_WIDTH;
	uc_priv->ysize       = AMD_MMI_DC_HEIGHT;
	uc_priv->bpix        = AMD_MMI_DC_BPP;
	/*
	 * AVBUF format code 32 unpacks the low three bytes of each word as
	 * colour and ignores the top byte. Advertise VIDEO_X8R8G8B8 (word
	 * 0x00RRGGBB) so the padding lands in that ignored byte; the R<->B
	 * swap programmed into the blend CSC in dc_init() then maps the
	 * channels to the right outputs. Using VIDEO_RGBA8888 here instead
	 * puts the forced 0xff in a colour lane, tinting black red/blue.
	 */
	uc_priv->format      = VIDEO_X8R8G8B8;
	uc_priv->line_length = uc_priv->xsize * VNBYTES(uc_priv->bpix);
	uc_priv->fb_size     = uc_plat->size;

	/*
	 * This DC/DCDMA path is not cache-coherent -- CPU writes to the
	 * framebuffer must be flushed before DCDMA reads them.
	 */
	video_set_flush_dcache(dev, true);

	ret = dc_bringup_hw(dev, priv);
	if (ret)
		return ret;

	/*
	 * --- DPTX: HPD, link training ---
	 *
	 * Attempted before acquiring the DCDMA channel below: DPTX attach is
	 * the step most likely to fail (depends on an external cable/sink
	 * actually being present and responsive), so fail fast here rather
	 * than first probing DCDMA for a pixel pipeline that has nowhere to
	 * send its output.
	 */
	dev_dbg(dev, "DC: attaching DPTX (HPD + link training)...\n");
	ret = display_attach(dptx);
	if (ret)
		return ret;
	dev_dbg(dev, "DC: DPTX attach succeeded, link is up\n");

	ret = dc_start_dcdma(dev, priv, uc_priv, uc_plat);
	if (ret)
		return ret;

	return dc_report_video_stream(dptx, priv);
}

/**
 * amd_mmi_dc_bind() - reserve framebuffer memory
 * @dev: the amd_mmi_dc device
 *
 * Return: 0 if OK, -ve on error.
 */
static int amd_mmi_dc_bind(struct udevice *dev)
{
	struct video_uc_plat *uc_plat = dev_get_uclass_plat(dev);

	uc_plat->size = AMD_MMI_DC_WIDTH * AMD_MMI_DC_HEIGHT * VNBYTES(AMD_MMI_DC_BPP);

	return 0;
}

static const struct udevice_id amd_mmi_dc_ids[] = {
	{ .compatible = "amd,mmi-dc-1.0" },
	{ }
};

U_BOOT_DRIVER(amd_mmi_dc) = {
	.name        = "amd_mmi_dc",
	.id          = UCLASS_VIDEO,
	.of_match    = amd_mmi_dc_ids,
	.bind        = amd_mmi_dc_bind,
	.of_to_plat  = amd_mmi_dc_of_to_plat,
	.probe       = amd_mmi_dc_probe,
	.plat_auto   = sizeof(struct video_uc_plat),
	.priv_auto   = sizeof(struct amd_mmi_dc_priv),
	.flags       = DM_FLAG_PRE_RELOC,
};
