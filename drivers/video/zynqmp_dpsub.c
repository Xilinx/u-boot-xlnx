// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 Xilinx Inc.
 */

#include <common.h>
#include <cpu_func.h>
#include <dm.h>
#include <errno.h>
#include <video.h>
#include <dm/device_compat.h>

/**
 * struct zynqmp_dpsub_priv - Private structure
 * @dev: Device uclass for video_ops
 */
struct zynqmp_dpsub_priv {
	struct udevice *dev;
};

static int zynqmp_dpsub_probe(struct udevice *dev)
{
	/* Only placeholder for power domain driver */
	return 0;
}

static const struct video_ops zynqmp_dpsub_ops = {
};

static const struct udevice_id zynqmp_dpsub_ids[] = {
	{ .compatible = "xlnx,zynqmp-dpsub-1.7" },
	{ }
};

U_BOOT_DRIVER(zynqmp_dpsub_video) = {
	.name = "zynqmp_dpsub_video",
	.id = UCLASS_VIDEO,
	.of_match = zynqmp_dpsub_ids,
	.ops = &zynqmp_dpsub_ops,
	.plat_auto = sizeof(struct video_uc_plat),
	.probe = zynqmp_dpsub_probe,
	.priv_auto = sizeof(struct zynqmp_dpsub_priv),
};
