/*
 * ML403.h: ML403 specific config options
 *
 * http://www.xilinx.com/ml403
 *
 * Derived from : ml300.h
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
 *     FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE FOR
 *     OBTAINING ANY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
 *     XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
 *     THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY
 *     WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM
 *     CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND
 *     FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 *     Xilinx products are not intended for use in life support appliances,
 *     devices, or systems. Use in such applications is expressly prohibited.
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

#ifndef __CONFIG_H
#define __CONFIG_H

/* #define DEBUG */
/* #define ET_DEBUG 1 */

/*
 * High Level Configuration Options
 * (easy to change)
 */

#define CONFIG_405		1	/* This is a PPC405 CPU		*/
#define CONFIG_4xx		1	/* ...member of PPC4xx family	*/
#define CONFIG_XILINX_ML403	1	/* ...on a Xilinx ML403 board	*/

#include "../board/xilinx/ml403/xparameters.h"

/*  Make some configuration choices based on the hardware design
 *  specified in xparameters.h.
 */

#ifdef XPAR_SYSACE_0_DEVICE_ID
#define CONFIG_SYSTEMACE	1
#define CONFIG_DOS_PARTITION	1
#define CFG_SYSTEMACE_BASE	XPAR_SYSACE_0_BASEADDR
#define CFG_SYSTEMACE_WIDTH	XPAR_SYSACE_0_MEM_WIDTH
#define ADD_SYSTEMACE_CMDS      | CFG_CMD_FAT
#define RM_SYSTEMACE_CMDS
#else
#define ADD_SYSTEMACE_CMDS
#define RM_SYSTEMACE_CMDS       | CFG_CMD_FAT
#endif

#if 1  /* for the moment assume that we have Flash */
#define CFG_ENV_IS_IN_FLASH	1	/* environment is in FLASH */
#define ADD_FLASH_CMDS          | CFG_CMD_FLASH
#define RM_FLASH_CMDS
#else
#define ADD_FLASH_CMDS
#define RM_FLASH_CMDS           | CFG_CMD_FLASH
#define CFG_NO_FLASH            1
#endif

#ifdef XPAR_IIC_0_DEVICE_ID
#if ! defined(CFG_ENV_IS_IN_FLASH)
#define CFG_ENV_IS_IN_EEPROM	1	/* environment is in IIC EEPROM */
#endif
#define ADD_IIC_CMDS            | CFG_CMD_I2C
#define RM_IIC_CMDS
#else
#define ADD_IIC_CMDS
#define RM_IIC_CMDS             | CFG_CMD_I2C
#endif

#ifdef XPAR_EMAC_0_DEVICE_ID
#define CONFIG_ETHADDR          00:0a:35:00:22:01
#define ADD_NET_CMDS            | CFG_CMD_NET | CFG_CMD_DHCP
#define RM_NET_CMDS
#else
#define ADD_NET_CMDS
#define RM_NET_CMDS             | CFG_CMD_NET | CFG_CMD_DHCP
#endif

#if ! (defined(CFG_ENV_IS_IN_FLASH) || defined(CFG_ENV_IS_IN_EEPROM))
#define CFG_ENV_IS_NOWHERE      1       /* no space to store environment */
#define CFG_ENV_SIZE		1024
#define CFG_MONITOR_BASE	0x02000000
#endif

/* following are used only if env is in EEPROM */
#ifdef	CFG_ENV_IS_IN_EEPROM
#define CFG_I2C_EEPROM_ADDR             (0xA0 >> 1)
#define CFG_I2C_EEPROM_ADDR_LEN         1
#define CFG_I2C_EEPROM_ADDR_OVERFLOW    0x1
#define CFG_ENV_OFFSET                  0
#define CFG_ENV_SIZE                    256
#define CFG_EEPROM_PAGE_WRITE_BITS      5
#define CFG_EEPROM_PAGE_WRITE_DELAY_MS  5
#define CONFIG_ENV_OVERWRITE            1  /* writable ethaddr and serial# */
#define CFG_MONITOR_BASE                0x02000000
#endif

/* following are used only if env is in Flash */
#ifdef CFG_ENV_IS_IN_FLASH
#define CFG_FLASH_BASE		0xFF800000
#define CFG_MAX_FLASH_BANKS	1	/* max number of memory banks        */
#define CFG_MAX_FLASH_SECT	256	/* max number of sectors on one chip */
#define CFG_FLASH_ERASE_TOUT	120000	/* Timeout for Flash Erase (in ms)   */
#define CFG_FLASH_WRITE_TOUT	500	/* Timeout for Flash Write (in ms)   */
#define CFG_FLASH_CFI
#define CFG_FLASH_CFI_DRIVER
#define CFG_ENV_OFFSET          0x00780000
#define CFG_ENV_SIZE            0x00040000
#define CONFIG_ENV_OVERWRITE    1       /* writable ethaddr and serial# */
#define CFG_MONITOR_BASE	0xFFFC0000
#endif

#define CONFIG_BAUDRATE         9600
#define CONFIG_BOOTDELAY        5       /* autoboot after 5 seconds	*/

#define CONFIG_BOOTCOMMAND      "bootm ffe00000" /* autoboot command	*/

#define CONFIG_BOOTARGS         "console=ttyS0,9600 ip=off " \
                                "root=/dev/xsysace/disc0/part2 rw"

#define CONFIG_LOADS_ECHO       1       /* echo on for serial download	*/
#define CFG_LOADS_BAUD_CHANGE   1       /* allow baudrate change	*/

#define REMOVE_COMMANDS	       (CFG_CMD_LOADS | \
                                CFG_CMD_IMLS \
                                RM_IIC_CMDS \
                                RM_SYSTEMACE_CMDS \
                                RM_NET_CMDS \
                                RM_FLASH_CMDS)
#define CONFIG_COMMANDS	       ((CONFIG_CMD_DFL \
                                 ADD_IIC_CMDS \
                                 ADD_SYSTEMACE_CMDS \
                                 ADD_NET_CMDS \
                                 ADD_FLASH_CMDS) \
				& ~REMOVE_COMMANDS)

/* this must be included AFTER the definition of CONFIG_COMMANDS (if any) */
#include <cmd_confdefs.h>

/*
 * Miscellaneous configurable options
 */
#define CFG_LONGHELP		/* undef to save memory		*/
#define CFG_PROMPT	"=> "	/* Monitor Command Prompt	*/

#define CFG_CBSIZE	256	/* Console I/O Buffer Size	*/

#define CFG_PBSIZE (CFG_CBSIZE+sizeof(CFG_PROMPT)+16)	/* Print Buffer Size */
#define CFG_MAXARGS	16	/* max number of command args	*/
#define CFG_BARGSIZE	CFG_CBSIZE	/* Boot Argument Buffer Size	*/

#define CFG_MEMTEST_START	0x0400000	/* memtest works on	*/
#define CFG_MEMTEST_END		0x0C00000	/* 4 ... 12 MB in DRAM	*/

#define CFG_DUART_CHAN		0
#define CFG_NS16550_REG_SIZE   -4
#define CFG_NS16550             1
#define CFG_INIT_CHAN1	        1

/* The following table includes the supported baudrates */
#define CFG_BAUDRATE_TABLE  \
    {300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400}

#define CFG_LOAD_ADDR		0x400000	/* default load address */
#define CFG_EXTBDINFO		1	/* To use extended board_into (bd_t) */

#define CFG_HZ		1000	/* decrementer freq: 1 ms ticks */

/*-----------------------------------------------------------------------
 * Start addresses for the final memory configuration
 * (Set up by the startup code)
 * Please note that CFG_SDRAM_BASE _must_ start at 0
 */
#define CFG_SDRAM_BASE		0x00000000
#define CFG_MONITOR_LEN		(192 * 1024)	/* Reserve 196 kB for Monitor	*/
#define CFG_MALLOC_LEN		(1024 * 1024)	/* Reserve 128 kB for malloc()	*/

/*
 * For booting Linux, the board info and command line data
 * have to be in the first 8 MB of memory, since this is
 * the maximum mapped by the Linux kernel during initialization.
 */
#define CFG_BOOTMAPSZ		(8 << 20)	/* Initial Memory map for Linux */

/*-----------------------------------------------------------------------
 * Cache Configuration
 */
#define CFG_DCACHE_SIZE		16384	/* Virtex-II Pro PPC 405 CPUs	*/
#define CFG_CACHELINE_SIZE	32	/* ...			        */

/*-----------------------------------------------------------------------
 * Definitions for initial stack pointer and data area (in DPRAM)
 */

#define CFG_INIT_RAM_ADDR	0x800000  /* inside of SDRAM */
#define CFG_INIT_RAM_END	0x2000	  /* End of used area in RAM */
#define CFG_GBL_DATA_SIZE	128	  /* size in bytes reserved for initial data */
#define CFG_GBL_DATA_OFFSET    (CFG_INIT_RAM_END - CFG_GBL_DATA_SIZE)
#define CFG_INIT_SP_OFFSET	CFG_GBL_DATA_OFFSET

/*
 * Internal Definitions
 *
 * Boot Flags
 */
#define BOOTFLAG_COLD	0x01	/* Normal Power-On: Boot from FLASH	*/
#define BOOTFLAG_WARM	0x02	/* Software reboot			*/

#endif				/* __CONFIG_H */
