/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __CONFIG_PIKSIV3_MICROZED_PROD_H
#define __CONFIG_PIKSIV3_MICROZED_PROD_H

#define PIKSI_REV "microzed"

#define CONFIG_SYS_SDRAM_SIZE (1024 * 1024 * 1024)

#ifdef CONFIG_TPL_BUILD

/* Write to MIO 8 = unused, read from LED */
#define CONFIG_TPL_BOOTSTRAP_RX0_MIO 47
#define CONFIG_TPL_BOOTSTRAP_TX0_MIO 47
#define CONFIG_TPL_BOOTSTRAP_RX1_MIO 47
#define CONFIG_TPL_BOOTSTRAP_TX1_MIO  8

#endif

#include <configs/piksiv3_prod.h>

#endif /* __CONFIG_PIKSIV3_MICROZED_PROD_H */
