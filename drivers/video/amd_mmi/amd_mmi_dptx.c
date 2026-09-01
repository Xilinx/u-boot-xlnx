// SPDX-License-Identifier: GPL-2.0
/*
 * AMD MMI DisplayPort TX output-stage driver
 *
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 * Author: Pranav Sanwal <pranav.sanwal@amd.com>
 */

#define LOG_CATEGORY UCLASS_DISPLAY

#include <display.h>
#include <dm.h>
#include <errno.h>

#include <linux/io.h>
#include <linux/ioport.h>

#include "../dw_dp/dw_dp_core.h"

struct amd_mmi_dptx_priv {
	struct dw_dp core;
};

/*
 * Always train at 1-lane RBR, the DP spec's own "Fail Safe" combination.
 */
#define AMD_MMI_DPTX_LINK_RATE		DP_LINK_BW_1_62
#define AMD_MMI_DPTX_LANE_COUNT		1

/* DPCD link-rate codes are expressed in units of this many Mbps. */
#define AMD_MMI_DPTX_LINK_RATE_UNIT_MBPS	270

/**
 * amd_mmi_dptx_enable_stream() - struct dm_display_ops enable_stream op
 * @dev: the amd_mmi_dptx device
 * @timing: the caller's fixed video timing (timing->pixelclock.typ is the
 *	    actual measured pixel clock rate)
 *
 * Translates the generic struct display_timing into the core's own
 * register-level struct dw_dp_mode, then hands it off to the generic core.
 * hback_porch/hfront_porch (and v-equivalents) are recombined into
 * h_start/h_blank here because that's what the hardware's MSA registers
 * actually expect (start-of-active-video and total-blanking offsets, not
 * separate front/back porch values).
 *
 * Return: 0 if OK, -ve on error.
 */
static int amd_mmi_dptx_enable_stream(struct udevice *dev,
				      const struct display_timing *timing)
{
	struct amd_mmi_dptx_priv *priv = dev_get_priv(dev);
	struct dw_dp_mode mode = {
		.h_active = timing->hactive.typ,
		.h_sync   = timing->hsync_len.typ,
		.h_start  = timing->hback_porch.typ + timing->hsync_len.typ,
		.h_blank  = timing->hfront_porch.typ + timing->hsync_len.typ +
			    timing->hback_porch.typ,
		.v_active = timing->vactive.typ,
		.v_sync   = timing->vsync_len.typ,
		.v_start  = timing->vback_porch.typ + timing->vsync_len.typ,
		.v_blank  = timing->vfront_porch.typ + timing->vsync_len.typ +
			    timing->vback_porch.typ,
	};

	return dw_dp_core_start_video_stream(&priv->core, &mode,
					     timing->pixelclock.typ);
}

/**
 * amd_mmi_dptx_check_link_health() - struct dm_display_ops check_link_health op
 * @dev: the amd_mmi_dptx device
 *
 * Return: 0 if healthy (or recovered), -ve on failure.
 */
static int amd_mmi_dptx_check_link_health(struct udevice *dev)
{
	struct amd_mmi_dptx_priv *priv = dev_get_priv(dev);

	return dw_dp_core_check_link_health(&priv->core);
}

/**
 * amd_mmi_dptx_check_crc() - struct dm_display_ops check_crc op
 * @dev: the amd_mmi_dptx device
 *
 * Return: 0 if OK, -ve on error.
 */
static int amd_mmi_dptx_check_crc(struct udevice *dev)
{
	struct amd_mmi_dptx_priv *priv = dev_get_priv(dev);

	return dw_dp_core_check_crc(&priv->core);
}

/**
 * amd_mmi_dptx_get_link_params() - struct dm_display_ops get_link_params op
 * @dev: the amd_mmi_dptx device
 * @link_rate_mbps: returns the per-lane link rate in Mbps
 * @lane_count: returns the number of lanes trained
 * @rate_name: returns "RBR"/"HBR"/"HBR2"/"unknown"
 */
static void amd_mmi_dptx_get_link_params(struct udevice *dev,
					 u32 *link_rate_mbps, u8 *lane_count,
					 const char **rate_name)
{
	struct amd_mmi_dptx_priv *priv = dev_get_priv(dev);

	/* e.g. 0x06 -> 1620 */
	*link_rate_mbps = priv->core.link_rate *
			  AMD_MMI_DPTX_LINK_RATE_UNIT_MBPS;
	*lane_count = priv->core.lane_count;
	*rate_name = dw_dp_link_rate_name(priv->core.link_rate);
}

/**
 * amd_mmi_dptx_attach() - struct dm_display_ops attach op
 * @dev: the amd_mmi_dptx device
 *
 * Return: 0 if OK, -ve on error.
 */
static int amd_mmi_dptx_attach(struct udevice *dev)
{
	struct amd_mmi_dptx_priv *priv = dev_get_priv(dev);
	int ret;

	ret = dw_dp_core_attach(&priv->core, AMD_MMI_DPTX_LINK_RATE,
				AMD_MMI_DPTX_LANE_COUNT);
	if (ret)
		dw_dp_core_debug_status(&priv->core);

	return ret;
}

static const struct dm_display_ops amd_mmi_dptx_ops = {
	.attach			= amd_mmi_dptx_attach,
	.enable_stream		= amd_mmi_dptx_enable_stream,
	.check_link_health	= amd_mmi_dptx_check_link_health,
	.check_crc		= amd_mmi_dptx_check_crc,
	.get_link_params	= amd_mmi_dptx_get_link_params,
};

/**
 * amd_mmi_dptx_probe() - map the "dp" register resource
 * @dev: the amd_mmi_dptx device
 *
 * Return: 0 if OK, -ve on error.
 */
static int amd_mmi_dptx_probe(struct udevice *dev)
{
	struct amd_mmi_dptx_priv *priv = dev_get_priv(dev);
	struct resource res;
	int ret;

	ret = dev_read_resource_byname(dev, "dp", &res);
	if (ret)
		return ret;

	priv->core.base = ioremap(res.start, resource_size(&res));
	if (!priv->core.base)
		return -ENOMEM;

	priv->core.dev = dev;

	return 0;
}

static const struct udevice_id amd_mmi_dptx_ids[] = {
	{ .compatible = "amd,mmi-dptx-1.0" },
	{ }
};

U_BOOT_DRIVER(amd_mmi_dptx) = {
	.name		= "amd_mmi_dptx",
	.id		= UCLASS_DISPLAY,
	.of_match	= amd_mmi_dptx_ids,
	.probe		= amd_mmi_dptx_probe,
	.ops		= &amd_mmi_dptx_ops,
	.priv_auto	= sizeof(struct amd_mmi_dptx_priv),
};
