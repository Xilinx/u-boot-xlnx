// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2008
 * Sergei Poselenov, Emcraft Systems, sposelenov@emcraft.com.
 */

#include <config.h>
#include <init.h>
#include <asm/processor.h>
#include <asm/immap_85xx.h>
#include <fsl_ddr_sdram.h>
#include <asm/processor.h>
#include <asm/mmu.h>
#include <spd_sdram.h>
#include <linux/delay.h>

#if !defined(CONFIG_SPD_EEPROM)
/*
 * Autodetect onboard DDR SDRAM on 85xx platforms
 *
 * NOTE: Some of the hardcoded values are hardware dependant,
 *       so this should be extended for other future boards
 *       using this routine!
 */
phys_size_t fixed_sdram(void)
{
	struct ccsr_ddr __iomem *ddr =
		(struct ccsr_ddr __iomem *)(CFG_SYS_FSL_DDR_ADDR);

	/*
	 * Disable memory controller.
	 */
	ddr->cs0_config = 0;
	ddr->sdram_cfg = 0;

	ddr->cs0_bnds = CFG_SYS_DDR_CS0_BNDS;
	ddr->cs0_config = CFG_SYS_DDR_CS0_CONFIG;
	ddr->timing_cfg_0 = CFG_SYS_DDR_TIMING_0;
	ddr->timing_cfg_1 = CFG_SYS_DDR_TIMING_1;
	ddr->timing_cfg_2 = CFG_SYS_DDR_TIMING_2;
	ddr->sdram_mode = CFG_SYS_DDR_MODE;
	ddr->sdram_interval = CFG_SYS_DDR_INTERVAL;
	ddr->sdram_cfg_2 = CFG_SYS_DDR_CONFIG_2;
	ddr->sdram_clk_cntl = CFG_SYS_DDR_CLK_CONTROL;

	asm ("sync;isync;msync");
	udelay(1000);

	ddr->sdram_cfg = CFG_SYS_DDR_CONFIG;
	asm ("sync; isync; msync");
	udelay(1000);

	if (get_ram_size(0, CFG_SYS_SDRAM_SIZE<<20) == CFG_SYS_SDRAM_SIZE<<20) {
		/*
		 * OK, size detected -> all done
		 */
		return CFG_SYS_SDRAM_SIZE<<20;
	}

	return 0;				/* nothing found !		*/
}
#endif

#if defined(CFG_SYS_DRAM_TEST)
int testdram(void)
{
	uint *pstart = (uint *) CONFIG_SYS_MEMTEST_START;
	uint *pend = (uint *) CONFIG_SYS_MEMTEST_END;
	uint *p;

	printf ("SDRAM test phase 1:\n");
	for (p = pstart; p < pend; p++)
		*p = 0xaaaaaaaa;

	for (p = pstart; p < pend; p++) {
		if (*p != 0xaaaaaaaa) {
			printf ("SDRAM test fails at: %08x\n", (uint) p);
			return 1;
		}
	}

	printf ("SDRAM test phase 2:\n");
	for (p = pstart; p < pend; p++)
		*p = 0x55555555;

	for (p = pstart; p < pend; p++) {
		if (*p != 0x55555555) {
			printf ("SDRAM test fails at: %08x\n", (uint) p);
			return 1;
		}
	}

	printf ("SDRAM test passed.\n");
	return 0;
}
#endif
