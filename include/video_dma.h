/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Generic metadata contract for a DMA channel that feeds a video/display
 * controller's pixel FIFO from a framebuffer in memory.
 *
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 *
 * Author: Pranav Sanwal <pranav.sanwal@amd.com>
 */

#ifndef _VIDEO_DMA_H_
#define _VIDEO_DMA_H_

#include <linux/types.h>

/**
 * enum video_dma_mode - transfer mode for a video-feeding dma_send()
 * @VIDEO_DMA_ONESHOT: fetch the descriptor once and stop
 * @VIDEO_DMA_CYCLIC: descriptor self-loops so the channel free-runs,
 *		      continuously re-fetching the same buffer
 */
enum video_dma_mode {
	VIDEO_DMA_ONESHOT,
	VIDEO_DMA_CYCLIC,
};

/**
 * struct video_dma_meta - metadata passed as the @metadata arg to dma_send()
 * @line_size: active bytes per scanline
 * @stride: bytes between the start of consecutive scanlines (line pitch)
 * @mode: see enum video_dma_mode
 * @wait_vsync: optional callback that blocks until a fresh VSYNC edge is
 *		observed, returning 0 on success or a negative error on
 *		timeout. Called immediately before each trigger attempt so
 *		the trigger lands right after a VSYNC edge instead of at an
 *		arbitrary phase. NULL if the caller has no VSYNC source to
 *		gate on.
 * @vsync_ctx: opaque context passed to @wait_vsync
 */
struct video_dma_meta {
	u32 line_size;
	u32 stride;
	enum video_dma_mode mode;
	int (*wait_vsync)(void *ctx);
	void *vsync_ctx;
};

#endif /* _VIDEO_DMA_H_ */
