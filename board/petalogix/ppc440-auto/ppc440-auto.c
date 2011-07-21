/*
 * (C) Copyright 2007 Michal Simek
 *
 * Michal  SIMEK <monstr@monstr.eu>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* This is a board specific file.  It's OK to include board specific
 * header files */

#include <common.h>
#include <config.h>
#include <netdev.h>

ulong __get_PCI_freq(void)
{
	return 0;
}

ulong get_PCI_freq(void) __attribute__((weak, alias("__get_PCI_freq")));

phys_size_t __initdram(int board_type)
{
	return get_ram_size(XILINX_RAM_START, XILINX_RAM_SIZE);
}
phys_size_t initdram(int) __attribute__((weak, alias("__initdram")));

void __get_sys_info(sys_info_t *sysInfo)
{
	/* FIXME */
	sysInfo->freqProcessor = XILINX_CLOCK_FREQ;
	sysInfo->freqPLB = XILINX_CLOCK_FREQ;
	sysInfo->freqPCI = 0;

	return;
}

void get_sys_info(sys_info_t *) __attribute__((weak, alias("__get_sys_info")));

#define stringify(foo) #foo

int __checkboard(void)
{
	/*puts(stringify(#XILINX_BOARD_NAME) "\n");*/
	return 0;
}
int checkboard(void) __attribute__((weak, alias("__checkboard")));


int gpio_init (void)
{
#ifdef CONFIG_SYS_GPIO_0
	*((unsigned long *)(CONFIG_SYS_GPIO_0_ADDR)) = 0xFFFFFFFF;
#endif
	return 0;
}

int board_eth_init(bd_t *bis)
{
	/*
	 * This board either has PCI NICs or uses the CPU's TSECs
	 * pci_eth_init() will return 0 if no NICs found, so in that case
	 * returning -1 will force cpu_eth_init() to be called.
	 */
#ifdef CONFIG_XILINX_EMACLITE
	return xilinx_emaclite_initialize(bis);
#endif
#ifdef CONFIG_XILINX_LL_TEMAC
	return xilinx_ll_temac_initialize(bis);
#endif
}
