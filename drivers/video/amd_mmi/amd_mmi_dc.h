/* SPDX-License-Identifier: GPL-2.0 */
/*
 * AMD MMI Display Controller (DC) - timing constants, register definitions,
 * and register accessors
 *
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Pranav Sanwal <pranav.sanwal@amd.com>
 */

#ifndef _AMD_MMI_DC_H_
#define _AMD_MMI_DC_H_

#include <dma.h>
#include <video.h>

#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/types.h>

/*
 * 640x480@60 Hz (VESA DMT v1.13 "VGA" timing) timing constants
 *
 * This is the DP spec's own "Fail Safe" mode -- DP 1.4 spec 5.2.1.2: "All
 * detachable DP Sink devices shall support 640x480 @ 60Hz as the Fail Safe
 * mode".
 */
#define AMD_MMI_DC_WIDTH	640
#define AMD_MMI_DC_HEIGHT	480
#define AMD_MMI_DC_BPP		VIDEO_BPP32	/* RGBA8888 */

/*
 * DT graph port index of mmi_dc's first DPTX-facing output port
 * (ports/port@c, reg = <12>) -- ports 0-11 are reserved for other,
 * currently-disabled DC connections. Matches Linux's
 * MMI_DC_DPTX_PORT_0 (drivers/gpu/drm/xlnx/mmi_dc.h).
 */
#define AMD_MMI_DC_DPTX_PORT	12

#define DC_H_ACTIVE		AMD_MMI_DC_WIDTH
#define DC_H_BLANK		160
#define DC_H_SYNC		96
#define DC_H_TOTAL		800
#define DC_H_START		144

#define DC_V_ACTIVE		AMD_MMI_DC_HEIGHT
#define DC_V_BLANK		45
#define DC_V_SYNC		2
#define DC_V_TOTAL		525
#define DC_V_START		35

#define DC_PIXEL_CLK_HZ		25175000UL	/* 25.175 MHz for 640x480@60 */

#define DC_DP_HTOTAL		0x0
#define DC_DP_VTOTAL		0x4
#define DC_DP_HSWIDTH		0xc
#define DC_DP_VSWIDTH		0x10
#define DC_DP_HRES		0x14
#define DC_DP_VRES		0x18
#define DC_DP_HSTART		0x1c
#define DC_DP_VSTART		0x20
#define DC_DP_MISC0		0x24
#define DC_DP_BPC_8		BIT(5)

#define DC_BLEND_NUM_RGB_CHANNELS	3
#define DC_BLEND_BG_CLR(n)		(0x0 + 4 * (n))
#define DC_BLEND_GLOBAL_ALPHA		0xc
#define DC_BLEND_OUTPUT_FMT		0x14
#define DC_BLEND_CSC_COEFF(n)		(0x20 + 4 * (n))
#define DC_BLEND_OUTCSC_OFF(n)		(0x74 + 4 * (n))

/* Layer 0 (video plane 0) input-side control, V_BLEND_LAYER0_CONTROL */
#define DC_BLEND_LAYER0_CTRL		0x18
#define DC_BLEND_LAYER_RGB_MODE		BIT(1)

/* Identity 3x3 CSC (pass-through RGB, Q3.12 fixed-point) */
#define DC_BLEND_CSC_IDENTITY_DIAG	0x1000
#define DC_BLEND_CSC_IDENTITY_OFF	0x0

#define DC_AVBUF_FORMAT			0x0
#define DC_AVBUF_FMT_RGBA8888		32

/* AV_CHBUF0: video plane 0 / DCDMA channel 0 buffer control */
#define DC_AVBUF_CHBUF0			0x10
#define DC_AVBUF_CHBUF_EN		BIT(0)
#define DC_AVBUF_CHBUF_FLUSH		BIT(1)
#define DC_AVBUF_CHBUF_BURST_MAX	(0xf << 2)

/*
 * AV_BUF_OUTPUT_AUDIO_VIDEO_SELECT: per-layer 2-bit pixel source select
 *
 * 'l' is evaluated twice below; only ever called with the constant 0
 * (layer 0) in this driver, so no side-effect/reuse hazard in practice.
 */
#define DC_AVBUF_OUT_AV_SEL		0x70
#define DC_AVBUF_OUT_AV_SEL_MASK(l)	GENMASK(2 * (l) + 1, 2 * (l))
#define DC_AVBUF_OUT_AV_SEL_MEM(l)	BIT(2 * (l))

#define DC_AVBUF_NON_LIVE_LATENCY	0x8
#define DC_AVBUF_NON_LIVE_LATENCY_VAL	0x20138
#define DC_AVBUF_CLK_SOURCE		0x120
#define DC_AVBUF_CLK_INTERNAL		BIT(2)
#define DC_AVBUF_SRST			0x124
#define DC_AVBUF_SRST_ASSERT		BIT(1)

#define DC_MISC_BYPASS		0xc4c
#define DC_MISC_VID_CLK		0xc5c
#define DC_MISC_VID_CLK_PS	BIT(1)
#define DC_MISC_WPROTS		0xc70
#define DC_MISC_FRAME_SWITCH	0xd80
#define DC_MISC_FRAME_SW_ALL	0x3f

#define DC_IRQ_DP_INT_STATUS		0x0	/* sticky, write-1-to-clear */
#define DC_IRQ_DP_INT_VSYNC_TS		BIT(31)
#define DC_IRQ_DP_INT_CHBUF0_UNDERFLW	BIT(16)
#define DC_IRQ_DP_INT_CHBUF0_OVERFLW	BIT(27)
#define DC_IRQ_DP_INT_VIDTILE_FMT_ERR	BIT(0)

struct amd_mmi_dc_priv {
	/*
	 * Set in probe(), used for dev_dbg()/dev_err() in helpers that only
	 * receive priv, not the udevice itself (dc_dump_state(), etc.).
	 */
	struct udevice	*dev;

	/* DC register regions (by name from DT) */
	void __iomem	*dp_base;
	void __iomem	*blend_base;
	void __iomem	*avbuf_base;
	void __iomem	*misc_base;
	void __iomem	*irq_base;

	/* DCDMA channel handle (channel 0 = vid.0.0) */
	struct dma	dma;

	ulong		pixel_clk_hz;
};

#define DC_DP_RD(p, reg)		readl((p)->dp_base + (reg))
#define DC_DP_WR(p, reg, val)		writel((val), (p)->dp_base + (reg))

#define DC_BLEND_RD(p, reg)		readl((p)->blend_base + (reg))
#define DC_BLEND_WR(p, reg, val)	writel((val), (p)->blend_base + (reg))

#define DC_AVBUF_RD(p, reg)		readl((p)->avbuf_base + (reg))
#define DC_AVBUF_WR(p, reg, val)	writel((val), (p)->avbuf_base + (reg))

#define DC_MISC_RD(p, reg)		readl((p)->misc_base + (reg))
#define DC_MISC_WR(p, reg, val)		writel((val), (p)->misc_base + (reg))

#define DC_IRQ_RD(p, reg)		readl((p)->irq_base + (reg))
#define DC_IRQ_WR(p, reg, val)		writel((val), (p)->irq_base + (reg))

#endif /* _AMD_MMI_DC_H_ */
