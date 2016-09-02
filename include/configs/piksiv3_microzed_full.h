/*
 * (C) Copyright 2013 Xilinx, Inc.
 *
 * Configuration for Micro Zynq Evaluation and Development Board - MicroZedBoard
 * See zynq-common.h for Zynq common configs
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_PIKSIV3_MICROZED_FULL_H
#define __CONFIG_PIKSIV3_MICROZED_FULL_H

#define CONFIG_SYS_SDRAM_SIZE		(1024 * 1024 * 1024)

#define CONFIG_SYS_NO_FLASH

#define CONFIG_ZYNQ_SDHCI0
#define CONFIG_ZYNQ_USB

/* ATAG support */
#define CONFIG_CMDLINE_TAG

#include <configs/zynq-common.h>

#endif /* __CONFIG_PIKSIV3_MICROZED_FULL_H */
