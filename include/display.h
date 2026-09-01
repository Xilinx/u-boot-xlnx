/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2014 Google Inc.
 */

#ifndef _DISPLAY_H
#define _DISPLAY_H

#include <linux/types.h>

struct udevice;
struct display_timing;

/**
 * Display uclass platform data for each device
 *
 * @source_id:	ID for the source of the display data, typically a video
 * controller
 * @src_dev:	Source device providing the video
 * @in_use:	Display is being used
 */
struct display_plat {
	int source_id;
	struct udevice *src_dev;
	bool in_use;
};

/**
 * display_read_timing() - Read timing information
 *
 * @dev:	Device to read from
 * Return: 0 if OK, -ve on error
 */
int display_read_timing(struct udevice *dev, struct display_timing *timing);

/**
 * display_port_enable() - Enable a display port device
 *
 * @dev:	Device to enable
 * @panel_bpp:	Number of bits per pixel for panel
 * @timing:	Display timings
 * Return: 0 if OK, -ve on error
 */
int display_enable(struct udevice *dev, int panel_bpp,
		   const struct display_timing *timing);

/**
 * display_in_use() - Check if a display is in use by any device
 *
 * Return: true if the device is in use (display_enable() has been called
 * successfully), else false
 */
bool display_in_use(struct udevice *dev);

/**
 * struct dm_display_ops - operations for the display uclass
 *
 * @read_timing: Read timing information directly into the given
 *	struct display_timing. Returns 0 if OK, -ve on error.
 * @read_edid: Read information from EDID into the given buffer (should
 *	be EDID_SIZE bytes). Returns the number of bytes read, <=0 for
 *	error.
 * @enable: Enable the display port device at the given bits-per-pixel
 *	and timing. Returns 0 if OK, -ve on error.
 * @mode_valid: Check if the given timing is supported. Returns true if
 *	supported, false if not.
 * @attach: Bring up the physical link (e.g. cable/hotplug detection and
 *	any link training needed), without enabling the video stream.
 *	Optional; needed by output stages whose @enable cannot combine
 *	link-bringup and video-stream-enable in a single call, because a
 *	real pixel source must already be feeding the hardware before the
 *	video stream is enabled -- see @enable_stream. Returns 0 if OK,
 *	-ve on error.
 * @enable_stream: Enable the video stream, using the given timing, on a
 *	device already brought up via @attach, once a real pixel source
 *	is already feeding the hardware. Optional; the companion to
 *	@attach for output stages that need the physical link brought up
 *	separately from enabling the video stream itself. The timing's
 *	pixel clock field must be the actual measured pixel clock, not a
 *	nominal request, since a mismatch between the requested and
 *	actual rate can cause drift or artifacts even though the link
 *	itself trained successfully. Returns 0 if OK, -ve on error.
 * @check_link_health: Re-check that the physical link is still healthy,
 *	attempting recovery if it is not. Optional; meant to be called at
 *	multiple checkpoints (e.g. after @attach, after @enable_stream,
 *	and again once the link has had time to settle) to catch a silent
 *	post-training link drop that a single check would miss. Returns 0
 *	if healthy (or recovered), -ve on failure.
 * @check_crc: Verify the sink is receiving correct per-frame pixel
 *	content, via a sink-side CRC test mode, if the sink and link
 *	support it. Optional; a no-op (not a failure) if unsupported.
 *	Returns 0 if OK (including the unsupported case), -ve on error.
 * @get_link_params: Get the currently trained link parameters -- the
 *	per-lane link rate in Mbps, the number of lanes trained, and a
 *	short human-readable rate name. Optional; only meaningful after
 *	@attach has returned 0.
 */
struct dm_display_ops {
	int (*read_timing)(struct udevice *dev, struct display_timing *timing);
	int (*read_edid)(struct udevice *dev, u8 *buf, int buf_size);
	int (*enable)(struct udevice *dev, int panel_bpp,
		      const struct display_timing *timing);
	bool (*mode_valid)(struct udevice *dev,
			   const struct display_timing *timing);
	int (*attach)(struct udevice *dev);
	int (*enable_stream)(struct udevice *dev,
			     const struct display_timing *timing);
	int (*check_link_health)(struct udevice *dev);
	int (*check_crc)(struct udevice *dev);
	void (*get_link_params)(struct udevice *dev, u32 *link_rate_mbps,
				u8 *lane_count, const char **rate_name);
};

#define display_get_ops(dev)	((struct dm_display_ops *)(dev)->driver->ops)

/**
 * display_attach() - Bring up the physical link without enabling video
 *
 * @dev:	Device to attach
 * Return: 0 if OK, -ENOSYS if not supported by this device, -ve on other error
 */
int display_attach(struct udevice *dev);

/**
 * display_enable_stream() - Enable the video stream on an attached device
 *
 * @dev:	Device to enable
 * @timing:	Display timings (timing->pixelclock.typ = actual pixel clock)
 * Return: 0 if OK, -ENOSYS if not supported by this device, -ve on other error
 */
int display_enable_stream(struct udevice *dev,
			  const struct display_timing *timing);

/**
 * display_check_link_health() - Re-check link health, recovering if needed
 *
 * @dev:	Device to check
 * Return: 0 if healthy (or recovered), -ENOSYS if not supported, -ve on
 *	   failure
 */
int display_check_link_health(struct udevice *dev);

/**
 * display_check_crc() - Verify per-frame pixel content via sink CRC
 *
 * @dev:	Device to check
 * Return: 0 if OK, -ENOSYS if not supported, -ve on error
 */
int display_check_crc(struct udevice *dev);

/**
 * display_get_link_params() - Get the currently trained link parameters
 *
 * @dev:	Device to query
 * @link_rate_mbps: Returns the per-lane link rate in Mbps
 * @lane_count:	Returns the number of lanes trained
 * @rate_name:	Returns a short human-readable rate name
 * Return: 0 if OK, -ENOSYS if not supported by this device
 */
int display_get_link_params(struct udevice *dev, u32 *link_rate_mbps,
			    u8 *lane_count, const char **rate_name);

#endif
