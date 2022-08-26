/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2016 - 2021 Xilinx, Inc.
 */

#include <linux/build_bug.h>

void mem_map_fill(void);

static inline int zynqmp_mmio_write(const u32 address, const u32 mask,
				    const u32 value)
{
	BUILD_BUG();
	return -EINVAL;
}
