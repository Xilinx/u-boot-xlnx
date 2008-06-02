/*
 * init.S: Stubs for U-Boot initialization
 *
 *     Author: Xilinx, Inc.
 *
 *
 *     This program is free software; you can redistribute it and/or modify it
 *     under the terms of the GNU General Public License as published by the
 *     Free Software Foundation; either version 2 of the License, or (at your
 *     option) any later version.
 *
 *
 *     XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
 *     COURTESY TO YOU. BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
 *     ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE, APPLICATION OR STANDARD,
 *     XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION IS FREE
 *     FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE FOR OBTAINING
 *     ANY THIRD PARTY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 *     XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 *     THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY
 *     WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM
 *     CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *     FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 *     Xilinx hardware products are not intended for use in life support
 *     appliances, devices, or systems. Use in such applications is
 *     expressly prohibited.
 *
 *
 *     (c) Copyright 2002-2004 Xilinx Inc.
 *     All rights reserved.
 *
 *
 *     You should have received a copy of the GNU General Public License along
 *     with this program; if not, write to the Free Software Foundation, Inc.,
 *     675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *
 */

#include <ppc_asm.tmpl>
#include <config.h>
#include <asm-ppc/mmu.h>

/**************************************************************************
 * TLB TABLE
 *
 * This table is used by the cpu boot code to setup the initial tlb
 * entries. Rather than make broad assumptions in the cpu source tree,
 * this table lets each board set things up however they like.
 *
 *  Pointer to the table is returned in r1
 *
 * The ML507 has 256MB of memory. We assume that it is mapped to address 0.
 * For the rest of the 4GB address range supported by EDK at this time
 * caches are turned off and the address space is guarded as peripherals
 * could be mapped anywhere.
 *
 *************************************************************************/

	.section .bootpg,"ax"
	.globl tlbtab

tlbtab:
	tlbtab_start

	tlbentry(0x00000000, SZ_256M, 0x00000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0x10000000, SZ_256M, 0x10000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0x20000000, SZ_256M, 0x20000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0x30000000, SZ_256M, 0x30000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0x40000000, SZ_256M, 0x40000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0x50000000, SZ_256M, 0x50000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0x60000000, SZ_256M, 0x60000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0x70000000, SZ_256M, 0x70000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0x80000000, SZ_256M, 0x80000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0x90000000, SZ_256M, 0x90000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0xa0000000, SZ_256M, 0xa0000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0xb0000000, SZ_256M, 0xb0000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0xc0000000, SZ_256M, 0xc0000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0xd0000000, SZ_256M, 0xd0000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0xe0000000, SZ_256M, 0xe0000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)
	tlbentry(0xf0000000, SZ_256M, 0xf0000000, 0, AC_R|AC_W|AC_X|SA_G|SA_I)

	tlbtab_end

#if 0
		.globl ext_bus_cntlr_init
ext_bus_cntlr_init:
	blr

	.globl sdram_init
sdram_init:
	blr
#endif
