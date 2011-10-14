/*
 * Hacked together,
 * hopefully functional.
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#define CONFIG_ARM1176		1 /* CPU */
//#define CONFIG_XDF		1 /* Board */
//#define CONFIG_DFE		1 /* Board sub-type ("flavor"?) */
#define CONFIG_PELE		1 /* SoC? */

#include "../board/petalogix/arm-auto/xparameters.h"

#undef CONFIG_PELE_XIL_LQSPI

#define	CONFIG_RTC_XPSSRTC


#define CONFIG_CMD_DATE		/* RTC? */


#define CONFIG_REGINFO		/* Again, debugging */
#undef CONFIG_CMD_SETGETDCR	/* README says 4xx only */

#define CONFIG_TIMESTAMP	/* print image timestamp on bootm, etc */

/* IPADDR, SERVERIP */
/* Need I2C for RTC? */

#define CONFIG_IDENT_STRING	"\nXilinx Pele Emulaton Platform"
#define CONFIG_PANIC_HANG	1 /* For development/debugging */

#undef CONFIG_SKIP_RELOCATE_UBOOT	

/* Uncomment it if you don't want Flash */
//#define CONFIG_SYS_NO_FLASH	

#define CONFIG_L2_OFF

#define CONFIG_PELE_IP_ENV	//this is to set ipaddr, ethaddr and serverip env variables.

/* CONFIG_SYS_MONITOR_BASE? */
/* CONFIG_SYS_MONITOR_LEN? */

#define CONFIG_SYS_CACHELINE_SIZE	32 /* Assuming bytes? */

/* CONFIG_SYS_INIT_RAM_ADDR? */
/* CONFIG_SYS_GLOBAL_DATA_OFFSET? */

/* Because (at least at first) we're going to be loaded via JTAG_Tcl */
//#define CONFIG_SKIP_LOWLEVEL_INIT	


/* HW to use */
#define CONFIG_XDF_UART	1
#define CONFIG_XDF_ETH	1
#define CONFIG_XDF_RTC	1
#define CONFIG_UART0	1

#if defined(CONFIG_UART0)
# define UART_BASE XPSS_UART0_BASEADDR
#elif defined(CONFIG_UART1)
# define UART_BASE XPSS_UART1_BASEADDR
#else
# error "Need to configure a UART (0 or 1)"
#endif


#define CONFIG_TTC0	1

/*
 * These were lifted straight from imx31_phycore, and may well be very wrong.
 */
//#define CONFIG_ENV_SIZE			4096
//#define CONFIG_ENV_SIZE			0x10000
#define CONFIG_NR_DRAM_BANKS		1
#define CONFIG_SYS_CBSIZE		256


/*
 * SPI Settings
 */
#define CONFIG_CMD_SPI
#define CONFIG_ENV_SPI_MAX_HZ   30000000
#define CONFIG_SF_DEFAULT_SPEED 30000000
#define CONFIG_SPI_FLASH
#define CONFIG_CMD_SF
#define CONFIG_XILINX_PSS_QSPI_USE_DUAL_FLASH
#ifdef NOTOW_BHILL
#define CONFIG_SPI_FLASH_ATMEL
#define CONFIG_SPI_FLASH_SPANSION
#define CONFIG_SPI_FLASH_WINBOND
#endif
#define CONFIG_SPI_FLASH_STMICRO

/*
 * NAND Flash settings
 */
#define CONFIG_CMD_NAND
#define CONFIG_CMD_NAND_LOCK_UNLOCK
#define CONFIG_SYS_MAX_NAND_DEVICE 1
#define CONFIG_SYS_NAND_BASE XPSS_NAND_BASEADDR
#define CONFIG_MTD_DEVICE
#define CONFIG_SYS_NAND_ONFI_DETECTION

/* Place a Xilinx Boot ROM header in u-boot image? */
#define CONFIG_PELE_XILINX_FLASH_HEADER

#ifdef CONFIG_PELE_XILINX_FLASH_HEADER
/* Address Xilinx boot rom should use to launch u-boot */
#ifdef CONFIG_PELE_XIL_LQSPI
#define CONFIG_PELE_XIP_START XPSS_QSPI_LIN_BASEADDR
#else
/* NOR */
#define CONFIG_PELE_XIP_START CONFIG_SYS_FLASH_BASE
#endif
#endif

/* Secure Digital */
#define CONFIG_MMC     1

#ifdef CONFIG_MMC
#define CONFIG_GENERIC_MMC
#define CONFIG_CMD_MMC
#define CONFIG_CMD_FAT
#define CONFIG_CMD_EXT2
#define CONFIG_DOS_PARTITION
#endif

#define BOARD_LATE_INIT	1

/*-----------------------------------------------------------------------
 * Main Memory 
 */

/* MALLOC */
#define CONFIG_SYS_MALLOC_LEN		0x400000

/* global pointer */
#define CONFIG_SYS_GBL_DATA_SIZE	128	/* size of global data */
/* start of global data */
//#define	CONFIG_SYS_GBL_DATA_OFFSET	(CONFIG_SYS_SDRAM_SIZE - CONFIG_SYS_GBL_DATA_SIZE)


/* Memory test handling */
#define	CONFIG_SYS_MEMTEST_START	CONFIG_SYS_SDRAM_BASE
#define	CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_SDRAM_BASE + 0x1000)

/*-----------------------------------------------------------------------
 * Physical Memory Map
 */
#define CONFIG_SYS_INIT_RAM_ADDR	CONFIG_SYS_SDRAM_BASE
#define CONFIG_SYS_INIT_RAM_SIZE	CONFIG_SYS_MALLOC_LEN
#define CONFIG_SYS_INIT_SP_ADDR		(CONFIG_SYS_INIT_RAM_ADDR + \
						CONFIG_SYS_INIT_RAM_SIZE - \
						GENERATED_GBL_DATA_SIZE)

/* Get common PetaLinux board setup */
#include <configs/petalinux-auto-board.h>

#undef CONFIG_PHYLIB

#endif /* __CONFIG_H */
