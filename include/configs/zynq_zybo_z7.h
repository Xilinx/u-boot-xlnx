/*
 * (C) Copyright 2012 Xilinx
 * (C) Copyright 2014 Digilent Inc.
 *
 * Configuration for Zynq Development Board - ZYBO Z7
 * See zynq-common.h for Zynq common configs
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_ZYNQ_ZYBO_Z7_H
#define __CONFIG_ZYNQ_ZYBO_Z7_H

#define CONFIG_ZYNQ_I2C0
#define CONFIG_ZYNQ_I2C1
#define CONFIG_SYS_I2C_EEPROM_ADDR_LEN	1
#define CONFIG_CMD_EEPROM
#define CONFIG_ZYNQ_GEM_EEPROM_ADDR	0x50
#define CONFIG_ZYNQ_GEM_I2C_MAC_OFFSET	0xFA
#define CONFIG_DISPLAY
#define CONFIG_I2C_EDID

/* Define ZYBO Z7 PS Clock Frequency to 33.3333MHz */
#define CONFIG_ZYNQ_PS_CLK_FREQ	33333333UL

#include <configs/zynq-common.h>

#endif /* __CONFIG_ZYNQ_ZYBO_H */
