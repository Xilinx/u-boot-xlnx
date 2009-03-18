/*
 * (C) Copyright 2007 Peter Ryser <peter.ryser@xilinx.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * http://www.xilinx.com/ml507
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
 *     (c) Copyright 2007 Xilinx Inc.
 *     All rights reserved.
 *
 *
 *     You should have received a copy of the GNU General Public License along
 *     with this program; if not, write to the Free Software Foundation, Inc.,
 *     675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/************************************************************************
 * ml507.h - configuration for Xilinx ML507 board based on
 *           Virtex-5 FXT FPGA with embedded PowerPC 440
 ***********************************************************************/

#ifndef __CONFIG_H
#define __CONFIG_H

/*CPU*/
#define CONFIG_440		1
#define CONFIG_XILINX_ML507	1
#include "../board/xilinx/ml507/xparameters.h"

/*Mem Map*/
#define CONFIG_SYS_SDRAM_SIZE_MB	256

#ifdef XPAR_FLASH_MEM0_BASEADDR
/*Env for Flash*/
/*
 * CONFIG_ENV_IS_IN_FLASH is defined in xilinx_ppc.h
 */
#define	CONFIG_ENV_SIZE		0x20000
#define	CONFIG_ENV_SECT_SIZE	0x20000
#define CONFIG_ENV_OFFSET	0x340000
#define CONFIG_ENV_ADDR		(XPAR_FLASH_MEM0_BASEADDR+CONFIG_ENV_OFFSET)
#define CONFIG_ENV_OVERWRITE	1

/*Flash*/
#define	CONFIG_SYS_FLASH_SIZE		(32*1024*1024)
#define	CONFIG_SYS_MAX_FLASH_SECT	259
#define CONFIG_SYS_FLASH_CFI		1
#define CONFIG_FLASH_CFI_DRIVER		1
#define MTDIDS_DEFAULT			"nor0=ml507-flash"
#define MTDPARTS_DEFAULT		"mtdparts=ml507-flash:-(user)"
#endif

#ifdef XPAR_IIC_0_BASEADDR
/*Env for EEPROM*/
/*
 * CONFIG_ENV_IS_IN_EEPROM is defined in xilinx_ppc.h
 */
#define CONFIG_SYS_EEPROM_SIZE 4096
#endif


/*Misc*/
#define CONFIG_SYS_PROMPT	"ml507:/# "	/* Monitor Command Prompt    */
#define CONFIG_PREBOOT		"echo U-Boot is up and runnining;"

/*Generic Configs*/
#include <configs/xilinx-ppc440.h>

#endif						/* __CONFIG_H */
