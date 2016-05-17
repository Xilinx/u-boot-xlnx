/*
 * (C) Copyright 2016 krtkl inc.
 *
 * Configuration for snickerdoodle black Zynq Development Board
 * See zynq-common.h for Zynq common configs
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_ZYNQ_SNICKERDOODLE_BLACK_H
#define __CONFIG_ZYNQ_SNICKERDOODLE_BLACK_H

#define CONFIG_SYS_SDRAM_SIZE		(1024 * 1024 * 1024)

#define CONFIG_ZYNQ_SERIAL_UART0
#define CONFIG_ZYNQ_GEM0
#define CONFIG_ZYNQ_GEM_PHY_ADDR0	0

#define CONFIG_SYS_NO_FLASH

#define CONFIG_ZYNQ_SDHCI0

#include <configs/zynq-common.h>

#endif /* __CONFIG_ZYNQ_SNICKERDOODLE_BLACK_H */
