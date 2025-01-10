/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2015 Stefan Roese <sr@denx.de>
 */
#ifndef __CONFIG_SOCFPGA_SR1500_H__
#define __CONFIG_SOCFPGA_SR1500_H__

#include <asm/arch/base_addr_ac5.h>

/* Memory configurations */
#define PHYS_SDRAM_1_SIZE		0x40000000	/* 1GiB on SR1500 */

/* Ethernet on SoC (EMAC) */
/* The PHY is autodetected, so no MII PHY address is needed here */

/* The rest of the configuration is shared */
#include <configs/socfpga_common.h>

#endif	/* __CONFIG_SOCFPGA_SR1500_H__ */
