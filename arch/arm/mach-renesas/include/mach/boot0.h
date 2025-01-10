/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Specialty padding for the R-Car Gen2 SPL JTAG loading
 */

#ifndef __BOOT0_H
#define __BOOT0_H

_start:
	ARM_VECTORS

#ifdef CONFIG_XPL_BUILD
	.word	0x0badc0d3;
	.word	0x0badc0d3;
	.word	0x0badc0d3;
	.word	0x0badc0d3;
	.word	0x0badc0d3;
	.word	0x0badc0d3;
	.word	0x0badc0d3;
	.word	0x0badc0d3;
#endif

#endif /* __BOOT0_H */
