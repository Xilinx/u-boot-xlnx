/*
  File: parallella.h
 
  Copyright (C) 2013 Adapteva, Inc.
  Contributed by Roman Trogan <support@adapteva.com>

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program (see the file COPYING).  If not, see
  <http://www.gnu.org/licenses/>.
*/

#ifndef __CONFIG_PARALLELLA_ZED_H
#define __CONFIG_PARALLELLA_ZED_H

#define PHYS_SDRAM_1_SIZE (1024 * 1024 * 1024)
#define CONFIG_SYS_MEM_TOP_HIDE 0x2000000

#define CONFIG_ZYNQ_SERIAL_UART1
#define CONFIG_PHY_ADDR	0

#define CONFIG_ZYNQ_GEM_OLD
#define CONFIG_XGMAC_PHY_ADDR CONFIG_PHY_ADDR
#define CONFIG_SYS_ENET

#define CONFIG_SYS_NO_FLASH

#define CONFIG_MMC
#define CONFIG_ZYNQ_SPI

/*
 * I2C support
 */
#define CONFIG_ZYNQ_I2C /* Support for Zynq I2C */
#define CONFIG_HARD_I2C /* I2C with hardware support */
#define CONFIG_ZYNQ_I2C_CTLR_0 /* Enable I2C_0 */
#define CONFIG_SYS_I2C_SPEED 100000 /* I2C speed and slave address */
#define CONFIG_CMD_I2C

#include <configs/zynq_common.h>

#undef  SD_BASEADDR
#define SD_BASEADDR    0xE0101000

#undef  CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS	\
        "ethaddr=04:4f:8b:00:00:00\0"   \
	"kernel_image=uImage\0"		       \
        "bitstream_image=parallella.bit.bin\0"	\
	"devicetree_image=devicetree.dtb\0"		\
	"kernel_size=0x500000\0"			\
	"devicetree_size=0x20000\0"			\
	"fdt_high=0x20000000\0"				\
	"initrd_high=0x20000000\0"			     \
	"qspiboot=echo Configuring PL and Booting Linux...;" \
		"mmcinfo;"					\
	        "fatload mmc 0 0x4000000 ${bitstream_image};"	\
	        "fpga load 0 0x4000000 0x3dbafc;"		\
		"fatload mmc 0 0x3000000 ${kernel_image};"	\
		"fatload mmc 0 0x2A00000 ${devicetree_image};"	\
		"bootm 0x3000000 - 0x2A00000\0"

#undef  CONFIG_BOOTDELAY 
#define CONFIG_BOOTDELAY  0 /* -2 to autoboot with no delay and not check for abort */

#endif /* __CONFIG_PARALLELLA_ZED_H */
