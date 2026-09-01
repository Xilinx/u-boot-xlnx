// SPDX-License-Identifier: GPL-2.0
/*
 * Test for 'video vidinfo' and 'video dispbars' commands
 *
 * Copyright (C) 2026 Advanced Micro Devices, Inc.
 * Author: Pranav Sanwal <pranav.sanwal@amd.com>
 */

#include <dm.h>
#include <video.h>

#include <dm/test.h>
#include <linux/kernel.h>
#include <test/cmd.h>
#include <test/ut.h>

/* Test 'video vidinfo' command */
static int cmd_test_vidinfo(struct unit_test_state *uts)
{
	ut_assertok(run_command("video vidinfo", 0));
	ut_assert_nextline("Device        : lcd");
	ut_assert_nextline("Resolution    : 1366 x 768");
	ut_assert_nextline("Bits per pixel: 16");
	ut_assert_nextline("Line length   : 2732 bytes");
	ut_assert_nextlinen("Framebuffer   :");
	ut_assert_console_end();

	return 0;
}
CMD_TEST(cmd_test_vidinfo, UTF_CONSOLE);

/* Matches dispbars_colours[] in cmd/video_test.c */
static const enum colour_idx dispbars_expect[] = {
	VID_WHITE,
	VID_LIGHT_RED,
	VID_LIGHT_GREEN,
	VID_LIGHT_BLUE,
	VID_YELLOW,
	VID_BLACK,
};

static u32 dispbars_read_pixel(struct video_priv *priv, int x, int y)
{
	ulong offset = (ulong)y * priv->line_length + (ulong)x * VNBYTES(priv->bpix);
	u8 *fb = priv->fb;

	if (priv->bpix == VIDEO_BPP16)
		return *(u16 *)(fb + offset);

	return *(u32 *)(fb + offset);
}

/* Test 'video dispbars' command */
static int cmd_test_dispbars(struct unit_test_state *uts)
{
	struct video_priv *priv;
	struct udevice *dev;
	unsigned int i;
	int xsize;

	ut_assertok(run_command("video dispbars", 0));
	ut_assert_nextline("dispbars: drawing 6 bars across 1366x768");
	ut_assert_console_end();

	ut_assertok(uclass_first_device_err(UCLASS_VIDEO, &dev));
	priv = dev_get_uclass_priv(dev);
	xsize = priv->xsize;

	/* Each bar spans [xsize*i/N, xsize*(i+1)/N); xsize*i/N is always inside it */
	for (i = 0; i < ARRAY_SIZE(dispbars_expect); i++) {
		int x = xsize * i / ARRAY_SIZE(dispbars_expect);
		u32 want = video_index_to_colour(priv, dispbars_expect[i]);
		u32 got = dispbars_read_pixel(priv, x, 0);

		ut_asserteq(want, got);
	}

	return 0;
}
CMD_TEST(cmd_test_dispbars, UTF_CONSOLE);
