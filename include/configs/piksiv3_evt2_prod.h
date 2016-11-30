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

#ifndef __CONFIG_PIKSIV3_EVT2_PROD_H
#define __CONFIG_PIKSIV3_EVT2_PROD_H

#define PIKSI_REV "evt2"

#define CONFIG_SYS_SDRAM_SIZE (512 * 1024 * 1024)

#define CONFIG_HW_WDT_DIS_MIO 0

#ifdef CONFIG_TPL_BUILD

#define CONFIG_TPL_BOOTSTRAP_RX0_MIO 10
#define CONFIG_TPL_BOOTSTRAP_TX0_MIO 11
#define CONFIG_TPL_BOOTSTRAP_RX1_MIO 13
#define CONFIG_TPL_BOOTSTRAP_TX1_MIO 12

#endif

#include <configs/piksiv3_prod.h>

#endif /* __CONFIG_PIKSIV3_EVT2_PROD_H */
