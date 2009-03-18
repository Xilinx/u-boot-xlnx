/*
 * (C) Copyright 2008
 * Ricado Ribalda-Universidad Autonoma de Madrid-ricardo.ribalda@uam.es
 * This work has been supported by: QTechnology  http://qtec.com/
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <config.h>
#include <common.h>
#include <i2c.h>
#include <asm/processor.h>

/* TODO: remove after debugging */
#include <linux/ctype.h>

ulong __get_PCI_freq(void)
{
	return 0;
}

ulong get_PCI_freq(void) __attribute__((weak, alias("__get_PCI_freq")));

int __board_pre_init(void)
{
	return 0;
}
int board_pre_init(void) __attribute__((weak, alias("__board_pre_init")));

int __checkboard(void)
{
	puts("Xilinx PPC405 Generic Board\n");
	return 0;
}
int checkboard(void) __attribute__((weak, alias("__checkboard")));

phys_size_t __initdram(int board_type)
{
	return get_ram_size(CONFIG_SYS_SDRAM_BASE,
			    CONFIG_SYS_SDRAM_SIZE_MB * 1024 * 1024);
}
phys_size_t initdram(int) __attribute__((weak, alias("__initdram")));

void __get_sys_info(sys_info_t *sysInfo)
{
	sysInfo->freqProcessor = XPAR_CORE_CLOCK_FREQ_HZ;
	sysInfo->freqPLB = XPAR_PLB_CLOCK_FREQ_HZ;
	sysInfo->freqPCI = 0;

	return;
}
void get_sys_info(sys_info_t *) __attribute__((weak, alias("__get_sys_info")));

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

void __board_get_enetaddr(uchar * enet)
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

	/* update the environment variable so that the fdt fixups for the emac
	 * addr will work
	 */
	sprintf (buff, "%02X:%02X:%02X:%02X:%02X:%02X",
 		 enet[0], enet[1], enet[2], enet[3], enet[4], enet[5]); 

	return;
}
void board_get_enetaddr(uchar * enet) __attribute__((weak, alias("__board_get_enetaddr")));
