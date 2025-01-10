// SPDX-License-Identifier: GPL-2.0+

#define LOG_CATEGORY UCLASS_FIRMWARE

#include <dm.h>

/* Firmware access is platform-dependent.  No generic code in uclass */
UCLASS_DRIVER(firmware) = {
	.id		= UCLASS_FIRMWARE,
	.name		= "firmware",
#if CONFIG_IS_ENABLED(OF_REAL)
	.post_bind	= dm_scan_fdt_dev,
#endif
};
