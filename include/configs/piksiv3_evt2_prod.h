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

#ifdef CONFIG_TPL_BUILD

/* Bootstrap pin configuration - nothing to do */
#define CONFIG_TPL_BOOTSTRAP_INIT

/* not implemented */
#define CONFIG_TPL_BOOTSTRAP_FAILSAFE false

/* not implemented */
#define CONFIG_TPL_BOOTSTRAP_ALTERNATE false

#endif

#include <configs/piksiv3_prod.h>

#endif /* __CONFIG_PIKSIV3_EVT2_PROD_H */
