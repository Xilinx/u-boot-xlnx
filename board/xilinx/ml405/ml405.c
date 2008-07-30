/*
 * ml405.c: U-Boot platform support for Xilinx ML403/405 board
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
 *     (c) Copyright 2002-2005 Xilinx Inc.
 *     All rights reserved.
 *
 *
 *     You should have received a copy of the GNU General Public License along
 *     with this program; if not, write to the Free Software Foundation, Inc.,
 *     675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <common.h>
#include <asm/processor.h>
#include "xparameters.h"

int
board_pre_init(void)
{
	return 0;
}

int
checkboard(void)
{
	uchar tmp[64];		/* long enough for environment variables */
	uchar *s, *e;
	int i = getenv_r("L", tmp, sizeof (tmp));

	if (i < 0) {
		printf("### No HW ID - assuming Virtex-4 FX12 or FX20");
	} else {
		for (e = tmp; *e; ++e) {
			if (*e == ' ')
				break;
		}

		printf("### Board Serial# is ");

		for (s = tmp; s < e; ++s) {
			putc(*s);
		}

	}
	putc('\n');

	return (0);
}

long int
initdram(int board_type)
{
	return 32 * 1024 * 1024;
}

int
testdram(void)
{
	printf("test: xxx MB - ok\n");

	return (0);
}

/* implement functions originally in cpu/ppc4xx/speed.c */
void
get_sys_info(sys_info_t * sysInfo)
{
	sysInfo->freqProcessor = XPAR_CORE_CLOCK_FREQ_HZ;

	/* only correct if the PLB and OPB run at the same frequency */
	sysInfo->freqPLB = XPAR_UARTNS550_0_CLOCK_FREQ_HZ;
	sysInfo->freqPCI = 0;
}
