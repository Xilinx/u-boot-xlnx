/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2016 - 2018 Xilinx, Inc.
 */

#ifndef _ASM_ARCH_SYS_PROTO_H
#define _ASM_ARCH_SYS_PROTO_H

#include <linux/build_bug.h>

enum tcm_mode {
	TCM_LOCK = 0,
	TCM_SPLIT = 1,
};

void initialize_tcm(enum tcm_mode mode);
void tcm_init(enum tcm_mode mode);
void mem_map_fill(void);

/* Overridable PMC multiboot accessor: weak MMIO default, firmware override */
u32 versal_pmc_multi_boot(void);
/* Direct MMIO read of the multiboot register (EL3 / no-firmware path) */
u32 versal_multi_boot_reg(void);

#endif /* _ASM_ARCH_SYS_PROTO_H */
