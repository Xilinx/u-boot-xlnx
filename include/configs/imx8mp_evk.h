/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright 2019 NXP
 */

#ifndef __IMX8MP_EVK_H
#define __IMX8MP_EVK_H

#include <linux/sizes.h>
#include <linux/stringify.h>
#include <asm/arch/imx-regs.h>

#define CFG_SYS_UBOOT_BASE	(QSPI0_AMBA_BASE + CONFIG_SYS_MMCSD_RAW_MODE_U_BOOT_SECTOR * 512)

#if defined(CONFIG_CMD_NET)
#define CFG_FEC_MXC_PHYADDR          1

#endif

#define BOOT_TARGET_DEVICES(func) \
       func(MMC, mmc, 1) \
       func(MMC, mmc, 2)

#include <config_distro_bootcmd.h>

/* Initial environment variables */
#define CFG_EXTRA_ENV_SETTINGS		\
	BOOTENV \
	"scriptaddr=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"kernel_addr_r=" __stringify(CONFIG_SYS_LOAD_ADDR) "\0" \
	"image=Image\0" \
	"console=ttymxc1,115200 earlycon=ec_imx6q,0x30890000,115200\0" \
	"fdt_addr_r=0x43000000\0"			\
	"boot_fdt=try\0" \
	"fdtfile=" CONFIG_DEFAULT_FDT_FILE "\0" \
	"initrd_addr=0x43800000\0"		\
	"bootm_size=0x10000000\0" \
	"mmcpart=1\0" \
	"mmcroot=/dev/mmcblk1p2 rootwait rw\0" \

/* Link Definitions */

#define CFG_SYS_INIT_RAM_ADDR	0x40000000
#define CFG_SYS_INIT_RAM_SIZE	0x80000

/* Totally 6GB DDR */
#define CFG_SYS_SDRAM_BASE		0x40000000
#define PHYS_SDRAM			0x40000000
#define PHYS_SDRAM_SIZE			0xC0000000	/* 3 GB */
#define PHYS_SDRAM_2			0x100000000
#define PHYS_SDRAM_2_SIZE		0xC0000000	/* 3 GB */

#endif
