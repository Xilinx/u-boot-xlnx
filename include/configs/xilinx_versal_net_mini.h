/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Configuration for Xilinx Versal NET MINI configuration
 *
 * (C) Copyright 2018-2022 Xilinx, Inc.
 * Michal Simek <michal.simek@amd.com>
 */

#ifndef __CONFIG_VERSAL_NET_MINI_H
#define __CONFIG_VERSAL_NET_MINI_H

#define CONFIG_EXTRA_ENV_SETTINGS

#include <configs/xilinx_versal_net.h>

/* Undef unneeded configs */
#undef CONFIG_EXTRA_ENV_SETTINGS

/* BOOTP options */
#undef CONFIG_BOOTP_BOOTFILESIZE
#undef CONFIG_BOOTP_MAY_FAIL

#undef CONFIG_SYS_CBSIZE
#define CONFIG_SYS_CBSIZE		1024

#endif /* __CONFIG_VERSAL_NET_MINI_H */
