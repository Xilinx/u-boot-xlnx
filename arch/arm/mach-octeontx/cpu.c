// SPDX-License-Identifier:    GPL-2.0
/*
 * Copyright (C) 2018 Marvell International Ltd.
 *
 * https://spdx.org/licenses
 */

#include <asm/armv8/mmu.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/board.h>

DECLARE_GLOBAL_DATA_PTR;

#define OTX_MEM_MAP_USED 3

/* 1 for 83xx, +1 is end of list which needs to be empty */
#define OTX_MEM_MAP_MAX (OTX_MEM_MAP_USED + 1 + CONFIG_NR_DRAM_BANKS + 1)

static struct mm_region otx_mem_map[OTX_MEM_MAP_MAX] = {
	{
		.virt = 0x800000000000UL,
		.phys = 0x800000000000UL,
		.size = 0x40000000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE
	}, {
		.virt = 0x840000000000UL,
		.phys = 0x840000000000UL,
		.size = 0x40000000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE
	}, {
		.virt = 0x880000000000UL,
		.phys = 0x880000000000UL,
		.size = 0x40000000000UL,
		.attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
			 PTE_BLOCK_NON_SHARE
	}

};

struct mm_region *mem_map = otx_mem_map;

void mem_map_fill(void)
{
	int banks = OTX_MEM_MAP_USED;
	u32 dram_start = CONFIG_TEXT_BASE;

	if (otx_is_soc(CN83XX)) {
		otx_mem_map[banks].virt = 0x8c0000000000UL;
		otx_mem_map[banks].phys = 0x8c0000000000UL;
		otx_mem_map[banks].size = 0x40000000000UL;
		otx_mem_map[banks].attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) |
					   PTE_BLOCK_NON_SHARE;
		banks = banks + 1;
	}

	for (int i = 0; i < CONFIG_NR_DRAM_BANKS; i++) {
		otx_mem_map[banks].virt = dram_start;
		otx_mem_map[banks].phys = dram_start;
		otx_mem_map[banks].size = gd->ram_size;
		otx_mem_map[banks].attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) |
					   PTE_BLOCK_NON_SHARE;
		banks = banks + 1;
	}
}

u64 get_page_table_size(void)
{
	return 0x80000;
}

void reset_cpu(void)
{
}
