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
#include <i2c.h>
#include <asm/processor.h>
#include "xparameters.h"

/*-----------------------------------------------------------------------------
 * aschex_to_byte --
 *-----------------------------------------------------------------------------
 */
static unsigned char aschex_to_byte (unsigned char *cp)
{
	u_char byte, c;

	c = *cp++;

	if ((c >= 'A') && (c <= 'F')) {
		c -= 'A';
		c += 10;
	} else if ((c >= 'a') && (c <= 'f')) {
		c -= 'a';
		c += 10;
	} else {
		c -= '0';
	}

	byte = c * 16;

	c = *cp;

	if ((c >= 'A') && (c <= 'F')) {
		c -= 'A';
		c += 10;
	} else if ((c >= 'a') && (c <= 'f')) {
		c -= 'a';
		c += 10;
	} else {
		c -= '0';
	}

	byte += c;

	return (byte);
}

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

phys_size_t
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

/*-----------------------------------------------------------------------------
 * board_get_enetaddr -- Read the MAC Address in the I2C EEPROM
 *-----------------------------------------------------------------------------
 */
void board_get_enetaddr (uchar * enet)
{
	int i;
	char buff[12];

	/* read the mac address from the i2c eeprom, the address 0x37
	   appears to be off by 1 according to factory documentation,
	   but not according to contents read?
	*/

	i2c_read(0x50, 0x37, 1, buff, 12);

	/* if the emac address in the i2c eeprom is not valid, then
	   then initialize it to a valid address, all xilinx addresses
	   have a known 1st several digits
	*/
	if ((buff[0] != 0x30) || (buff[1] != 0x30) || (buff[2] != 0x30) ||
	    (buff[3] != 0x41) || (buff[4] != 0x33) || (buff[5] != 0x35))
	{
		enet[0] = 0x00;
		enet[1] = 0x0A;
		enet[2] = 0x35;
		enet[3] = 0x01;
		enet[4] = 0x02;		
		enet[5] = 0x03;
		printf("MAC address not valid from I2C EEPROM, set to default\n");
	}
	else
	{
		/* convert the mac address from i2c eeprom from ascii hex to 
		   binary */

		for (i = 0; i < 6; i++) {
			enet[i] = aschex_to_byte ((unsigned char *)&buff[i*2]);
		}
		printf("MAC address valid from I2C EEPROM\n");
	}

	printf ("MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
		enet[0], enet[1], enet[2], enet[3], enet[4], enet[5]);

	return;
}
