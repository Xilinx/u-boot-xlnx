// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2013 Freescale Semiconductor, Inc.
 */

#include <config.h>
#include <asm/fsl_law.h>
#include <asm/mmu.h>

struct law_entry law_table[] = {
#ifdef CONFIG_MTD_NOR_FLASH
	SET_LAW(CFG_SYS_FLASH_BASE_PHYS, LAW_SIZE_256M, LAW_TRGT_IF_IFC),
#endif
#ifdef CFG_SYS_BMAN_MEM_PHYS
	SET_LAW(CFG_SYS_BMAN_MEM_PHYS, LAW_SIZE_32M, LAW_TRGT_IF_BMAN),
#endif
#ifdef CFG_SYS_QMAN_MEM_PHYS
	SET_LAW(CFG_SYS_QMAN_MEM_PHYS, LAW_SIZE_32M, LAW_TRGT_IF_QMAN),
#endif
#ifdef CFG_SYS_CPLD_BASE_PHYS
	SET_LAW(CFG_SYS_CPLD_BASE_PHYS, LAW_SIZE_128K, LAW_TRGT_IF_IFC),
#endif
#ifdef CFG_SYS_DCSRBAR_PHYS
	SET_LAW(CFG_SYS_DCSRBAR_PHYS, LAW_SIZE_4M, LAW_TRGT_IF_DCSR),
#endif
#ifdef CFG_SYS_NAND_BASE_PHYS
	SET_LAW(CFG_SYS_NAND_BASE_PHYS, LAW_SIZE_64K, LAW_TRGT_IF_IFC),
#endif
};

int num_law_entries = ARRAY_SIZE(law_table);
