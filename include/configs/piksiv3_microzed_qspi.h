/*
 * (C) Copyright 2013 Xilinx, Inc.
 *
 * Configuration for Micro Zynq Evaluation and Development Board - MicroZedBoard
 * See zynq-common.h for Zynq common configs
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_PIKSIV3_MICROZED_QSPI_H
#define __CONFIG_PIKSIV3_MICROZED_QSPI_H

#define CONFIG_SYS_SDRAM_SIZE		(1024 * 1024 * 1024)

#define CONFIG_SYS_NO_FLASH

/* ATAG support */
#define CONFIG_CMDLINE_TAG

/* Image table */
#define CONFIG_IMAGE_TABLE_BOOT
#define CONFIG_IMAGE_SET_OFFSET_FAILSAFE_A 0x000C0000U
#define CONFIG_IMAGE_SET_OFFSET_FAILSAFE_B 0x000D0000U
#define CONFIG_IMAGE_SET_OFFSET_STANDARD_A 0x000E0000U
#define CONFIG_IMAGE_SET_OFFSET_STANDARD_B 0x000F0000U
#define CONFIG_IMAGE_SET_OFFSETS          \
    {CONFIG_IMAGE_SET_OFFSET_FAILSAFE_A,  \
     CONFIG_IMAGE_SET_OFFSET_FAILSAFE_B,  \
     CONFIG_IMAGE_SET_OFFSET_STANDARD_A,  \
     CONFIG_IMAGE_SET_OFFSET_STANDARD_B}

/* CPU clock */
#ifndef CONFIG_CPU_FREQ_HZ
#define CONFIG_CPU_FREQ_HZ	800000000
#endif

/* Cache options */
#define CONFIG_CMD_CACHE
#define CONFIG_SYS_CACHELINE_SIZE	32

/* Timer */
#define ZYNQ_SCUTIMER_BASEADDR		0xF8F00600
#define CONFIG_SYS_TIMERBASE		ZYNQ_SCUTIMER_BASEADDR
#define CONFIG_SYS_TIMER_COUNTS_DOWN
#define CONFIG_SYS_TIMER_COUNTER	(CONFIG_SYS_TIMERBASE + 0x4)

/* Serial drivers */
#define CONFIG_BAUDRATE		115200
/* The following table includes the supported baudrates */
#define CONFIG_SYS_BAUDRATE_TABLE 	{CONFIG_BAUDRATE}

#define CONFIG_ARM_DCC
#define CONFIG_ZYNQ_SERIAL

/* QSPI */
#define CONFIG_SF_DEFAULT_SPEED	30000000
#define CONFIG_CMD_SF

/* Environment */
#define CONFIG_ENV_IS_NOWHERE
#define CONFIG_ENV_SIZE			(128 << 10)

/* Default environment */
#define CONFIG_EXTRA_ENV_SETTINGS	\
  "preboot=run img_tbl_boot\0" \
	"img_tbl_boot=" \
		"sf probe 0 0 0 && " \
		"sf read ${img_tbl_kernel_load_address} " \
		        "${img_tbl_kernel_flash_offset} " \
		        "${img_tbl_kernel_size} && " \
		"bootm ${img_tbl_kernel_load_address}\0"

#define CONFIG_BOOTCOMMAND		""

#define CONFIG_PREBOOT
#define CONFIG_BOOTDELAY		0 /* -1 to Disable autoboot */
#define CONFIG_SYS_LOAD_ADDR	0 /* default? */

/* Miscellaneous configurable options */
#define CONFIG_BOARD_LATE_INIT
#define CONFIG_DISPLAY_BOARDINFO
#define CONFIG_CLOCKS
#define CONFIG_SYS_MAXARGS		32 /* max number of command args */
#define CONFIG_SYS_CBSIZE		2048 /* Console I/O Buffer Size */
#define CONFIG_SYS_PBSIZE		(CONFIG_SYS_CBSIZE + \
					                   sizeof(CONFIG_SYS_PROMPT) + 16)

/* Physical Memory map */
#define CONFIG_SYS_TEXT_BASE    0x4000000
#define CONFIG_SYS_UBOOT_START  CONFIG_SYS_TEXT_BASE

#define CONFIG_NR_DRAM_BANKS		1
#define CONFIG_SYS_SDRAM_BASE		0

#define CONFIG_SYS_MEMTEST_START	CONFIG_SYS_SDRAM_BASE
#define CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_SDRAM_BASE + 0x1000)

#define CONFIG_SYS_MALLOC_LEN		0xC00000

#define CONFIG_SYS_INIT_RAM_ADDR	0xFFFF0000
#define CONFIG_SYS_INIT_RAM_SIZE	0x1000
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_INIT_RAM_ADDR + \
                                   CONFIG_SYS_INIT_RAM_SIZE - \
                                   GENERATED_GBL_DATA_SIZE)

/* Open Firmware flat tree */
#define CONFIG_OF_LIBFDT

/* FIT support */
#define CONFIG_IMAGE_FORMAT_LEGACY /* enable also legacy image format */

/* Extend size of kernel image for uncompression */
#define CONFIG_SYS_BOOTM_LEN	(60 * 1024 * 1024)

#define CONFIG_SYS_LDSCRIPT  "arch/arm/mach-zynq/u-boot.lds"

#define CONFIG_SYS_HZ			1000

/* For development/debugging */
#ifdef DEBUG
# define CONFIG_CMD_REGINFO
# define CONFIG_PANIC_HANG
#endif

/* SPL part */
#define CONFIG_SPL_FRAMEWORK
#define CONFIG_SPL_LIBCOMMON_SUPPORT
#define CONFIG_SPL_LIBGENERIC_SUPPORT
#define CONFIG_SPL_SERIAL_SUPPORT
#define CONFIG_SPL_BOARD_INIT

/* TPL optimizations */
#ifdef CONFIG_TPL_BUILD
#undef CONFIG_SPL_SERIAL_SUPPORT
#undef CONFIG_SPL_BOARD_INIT
#undef CONFIG_SPL_DM
#undef CONFIG_SPL_OF_CONTROL
#undef CONFIG_SPL_OF_EMBED
#endif

/* Disable dcache for SPL just for sure */
#ifdef CONFIG_SPL_BUILD
#define CONFIG_SYS_DCACHE_OFF
#undef CONFIG_FPGA
#endif

#ifdef CONFIG_TPL_BUILD

/* TPL is loaded to low OCM and relocated to high OCM */
/* 40kB code + bss, 8kB heap, 16kB stack in high OCM */

#define CONFIG_SPL_LDSCRIPT "arch/arm/mach-zynq/u-boot-spl-reloc.lds"

#define CONFIG_SPL_TEXT_BASE              0x00000000
#define CONFIG_SPL_RELOC_ADDR             0xffff0000
#define CONFIG_SPL_MAX_SIZE               0x0000a000

#define CONFIG_SYS_SPL_MALLOC_START       0xffffa000
#define CONFIG_SYS_SPL_MALLOC_SIZE        0x00002000

#define CONFIG_SPL_STACK                  0xfffffe00

/* Use image table */
#define CONFIG_SPL_BOARD_LOAD_IMAGE

/* Bootstrap pin configuration - nothing to do */
#define CONFIG_TPL_BOOTSTRAP_INIT

/* MIO 51 = SW1, default low */
#define CONFIG_TPL_BOOTSTRAP_FAILSAFE \
  ((readl(0xE000A064) & (1<<19)) ? true : false)

/* MIO 47 = LED, default low */
#define CONFIG_TPL_BOOTSTRAP_ALTERNATE \
  ((readl(0xE000A064) & (1<<15)) ? true : false)

#else

/* SPL is loaded to low OCM, no relocation */
/* 192kB code in low OCM */
/* 8kB heap, 8kB bss, 48kB stack in high OCM */

#define CONFIG_SPL_LDSCRIPT "arch/arm/mach-zynq/u-boot-spl.lds"

#define CONFIG_SPL_TEXT_BASE              0x00000000
#define CONFIG_SPL_MAX_SIZE               0x00030000

#define CONFIG_SYS_SPL_MALLOC_START       0xffff0000
#define CONFIG_SYS_SPL_MALLOC_SIZE        0x00002000

#define CONFIG_SPL_BSS_START_ADDR         0xffff2000
#define CONFIG_SPL_BSS_MAX_SIZE           0x00002000

#define CONFIG_SPL_STACK                  0xfffffe00

/* QSPI support */
#define CONFIG_SPL_SPI_SUPPORT
#define CONFIG_SPL_SPI_LOAD
#define CONFIG_SPL_SPI_FLASH_SUPPORT
#define CONFIG_SYS_SPI_U_BOOT_OFFS  0x100000

/* Use image table */
#define CONFIG_SPL_BOARD_LOAD_IMAGE

#endif

#endif /* __CONFIG_PIKSIV3_MICROZED_QSPI_H */
