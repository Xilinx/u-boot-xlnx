/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Configuration for Xilinx Versal NET QSPI Flash utility
 *
 * (C) Copyright 2022, Advanced Micro Devices, Inc.
 *
 * Michal Simek <michal.simek@amd.com>
 * Ashok Reddy Soma <ashok.reddy.soma@amd.com>
 */

#ifndef __CONFIG_VERSAL_NET_MINI_QSPI_H
#define __CONFIG_VERSAL_NET_MINI_QSPI_H

#include <configs/xilinx_versal_net_mini.h>

#undef CONFIG_SYS_INIT_SP_ADDR
#define CONFIG_SYS_INIT_SP_ADDR	(CONFIG_SYS_TEXT_BASE + 0x20000)

#endif /* __CONFIG_VERSAL_NET_MINI_QSPI_H */
