// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2011 Freescale Semiconductor, Inc.
 */

#include <config.h>
#include <clock_legacy.h>
#include <init.h>
#include <ns16550.h>
#include <asm/io.h>
#include <nand.h>
#include <linux/compiler.h>
#include <asm/fsl_law.h>
#include <fsl_ddr_sdram.h>
#include <asm/global_data.h>

DECLARE_GLOBAL_DATA_PTR;

void board_init_f(ulong bootflag)
{
	u32 plat_ratio;
	ccsr_gur_t *gur = (void *)CFG_SYS_MPC85xx_GUTS_ADDR;

#if defined(CFG_SYS_NAND_BR_PRELIM) && defined(CFG_SYS_NAND_OR_PRELIM)
	set_lbc_br(0, CFG_SYS_NAND_BR_PRELIM);
	set_lbc_or(0, CFG_SYS_NAND_OR_PRELIM);
#endif

	/* initialize selected port with appropriate baud rate */
	plat_ratio = in_be32(&gur->porpllsr) & MPC85xx_PORPLLSR_PLAT_RATIO;
	plat_ratio >>= 1;
	gd->bus_clk = get_board_sys_clk() * plat_ratio;

	ns16550_init((struct ns16550 *)CFG_SYS_NS16550_COM1,
		     gd->bus_clk / 16 / CONFIG_BAUDRATE);

	puts("\nNAND boot... ");

	/* copy code to RAM and jump to it - this should not return */
	/* NOTE - code has to be copied out of NAND buffer before
	 * other blocks can be read.
	 */
	relocate_code(CONFIG_SPL_RELOC_STACK, 0, CONFIG_SPL_RELOC_TEXT_BASE);
}

void board_init_r(gd_t *gd, ulong dest_addr)
{
	puts("\nSecond program loader running in sram...");
	nand_boot();
}

void putc(char c)
{
	if (c == '\n')
		ns16550_putc((struct ns16550 *)CFG_SYS_NS16550_COM1, '\r');

	ns16550_putc((struct ns16550 *)CFG_SYS_NS16550_COM1, c);
}

void puts(const char *str)
{
	while (*str)
		putc(*str++);
}
