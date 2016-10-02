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

#ifndef __CONFIG_PIKSIV3_FULL_H
#define __CONFIG_PIKSIV3_FULL_H

#define CONFIG_SYS_NO_FLASH

#define CONFIG_ZYNQ_SDHCI0
#define CONFIG_ZYNQ_USB

/* ATAG support */
#define CONFIG_CMDLINE_TAG

/* CRC verfication support */
#define CONFIG_HASH_VERIFY

 /* Factory data */
#define CONFIG_FACTORY_DATA
#define CONFIG_FACTORY_DATA_OFFSET 0x00040000U
#define CONFIG_FACTORY_DATA_FALLBACK
#define CONFIG_ZYNQ_GEM_FACTORY_ADDR

#include <configs/zynq-common.h>

#endif /* __CONFIG_PIKSIV3_FULL_H */
