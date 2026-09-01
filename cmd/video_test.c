// SPDX-License-Identifier: GPL-2.0
/*
 * video - test/debug commands for the active video device
 *
 * vidinfo  - print generic information about the active video device
 * dispbars - fill the framebuffer with test colour bars
 *
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 * Author: Pranav Sanwal <pranav.sanwal@amd.com>
 */

#include <command.h>
#include <dm.h>
#include <video.h>

#include <linux/kernel.h>

static int do_vidinfo(struct cmd_tbl *cmdtp, int flag, int argc,
		      char *const argv[])
{
	struct video_uc_plat *uc_plat;
	struct video_priv *priv;
	struct udevice *dev;
	int ret;

	ret = uclass_first_device_err(UCLASS_VIDEO, &dev);
	if (ret) {
		printf("No video device found: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	uc_plat = dev_get_uclass_plat(dev);
	priv = dev_get_uclass_priv(dev);

	printf("Device        : %s\n", dev->name);
	printf("Resolution    : %d x %d\n", priv->xsize, priv->ysize);
	printf("Bits per pixel: %d\n", VNBITS(priv->bpix));
	printf("Line length   : %d bytes\n", priv->line_length);
	printf("Framebuffer   : 0x%08lx, size %u bytes\n",
	       uc_plat->base, uc_plat->size);

	return CMD_RET_SUCCESS;
}

static const enum colour_idx dispbars_colours[] = {
	VID_WHITE,
	VID_LIGHT_RED,
	VID_LIGHT_GREEN,
	VID_LIGHT_BLUE,
	VID_YELLOW,
	VID_BLACK,
};

#define DISPBARS_NUM_COLOURS	ARRAY_SIZE(dispbars_colours)

static int do_dispbars(struct cmd_tbl *cmdtp, int flag, int argc,
		       char *const argv[])
{
	struct video_priv *priv;
	int xsize, ysize, ret;
	struct udevice *dev;
	unsigned int i;

	ret = uclass_first_device_err(UCLASS_VIDEO, &dev);
	if (ret) {
		printf("No video device found: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	priv = dev_get_uclass_priv(dev);
	xsize = video_get_xsize(dev);
	ysize = video_get_ysize(dev);

	printf("dispbars: drawing %d bars across %dx%d\n",
	       (int)DISPBARS_NUM_COLOURS, xsize, ysize);

	for (i = 0; i < DISPBARS_NUM_COLOURS; i++) {
		int xend = xsize * (i + 1) / DISPBARS_NUM_COLOURS;
		int xstart = xsize * i / DISPBARS_NUM_COLOURS;
		u32 colour = video_index_to_colour(priv, dispbars_colours[i]);

		ret = video_fill_part(dev, xstart, 0, xend, ysize, colour);
		if (ret) {
			printf("dispbars: fill failed: %d\n", ret);
			return CMD_RET_FAILURE;
		}
	}

	ret = video_sync(dev, true);
	if (ret) {
		printf("dispbars: sync failed: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}

U_BOOT_LONGHELP(video,
	"vidinfo  - print information about the active video device\n"
	"dispbars - fill the framebuffer with test colour bars and sync\n");

U_BOOT_CMD_WITH_SUBCMDS(video, "test/debug commands for the active video device", video_help_text,
	U_BOOT_SUBCMD_MKENT(vidinfo, 1, 1, do_vidinfo),
	U_BOOT_SUBCMD_MKENT(dispbars, 1, 1, do_dispbars));
