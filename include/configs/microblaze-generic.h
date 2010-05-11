/*
 * (C) Copyright 2007-2008 Michal Simek
 *
 * Michal SIMEK <monstr@monstr.eu>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#include "../board/xilinx/microblaze-generic/xparameters.h"

#define	CONFIG_MICROBLAZE	1	/* MicroBlaze CPU */
#define	MICROBLAZE_V5		1

/* uart, note 16550 is checked 1st as there can be a 16550 and a lite in the system
   for the mdm
*/
/* UARTLITE0 is used for MDM. Use UARTLITE1 for Microblaze */

#ifdef XPAR_UARTNS550_0_BASEADDR
	#define CONFIG_SYS_NS16550	1
	#define CONFIG_SYS_NS16550_SERIAL
	#define CONFIG_SYS_NS16550_REG_SIZE	-4
	#define CONFIG_CONS_INDEX	1
	#define CONFIG_SYS_NS16550_COM1	(XPAR_UARTNS550_0_BASEADDR + 0x1000 + 0x3)
	#define CONFIG_SYS_NS16550_CLK	XPAR_UARTNS550_0_CLOCK_FREQ_HZ
	#define	CONFIG_BAUDRATE		9600

	/* The following table includes the supported baudrates */
	#define CONFIG_SYS_BAUDRATE_TABLE  \
		{300, 600, 1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200, 230400}
	#define CONSOLE_ARG	"console=console=ttyS0,115200\0"
#elif XPAR_UARTLITE_1_BASEADDR
	#define	CONFIG_XILINX_UARTLITE
	#define	CONFIG_SERIAL_BASE	XPAR_UARTLITE_1_BASEADDR
	#define	CONFIG_BAUDRATE		XPAR_UARTLITE_1_BAUDRATE
	#define	CONFIG_SYS_BAUDRATE_TABLE	{ CONFIG_BAUDRATE }
	#define CONSOLE_ARG	"console=console=ttyUL0,115200\0"
#else
	#error Undefined uart
#endif

/* setting reset address */
/*#define	CONFIG_SYS_RESET_ADDRESS	TEXT_BASE*/

/* ethernet */
#ifdef XPAR_EMACLITE_0_BASEADDR
	#define XILINX_EMACLITE_BASEADDR XPAR_EMACLITE_0_BASEADDR
	#define CONFIG_XILINX_EMACLITE	1
	#define CONFIG_SYS_ENET
#elif XPAR_LLTEMAC_0_BASEADDR
	#define XILINX_LLTEMAC_BASEADDR XPAR_LLTEMAC_0_BASEADDR
	#if (XPAR_LLTEMAC_0_LLINK_CONNECTED_TYPE == XPAR_LL_DMA)
		#define XILINX_LLTEMAC_SDMA_CTRL_BASEADDR \
			XPAR_LLTEMAC_0_LLINK_CONNECTED_BASEADDR
	#elif (XPAR_LLTEMAC_0_LLINK_CONNECTED_TYPE == XPAR_LL_FIFO)
		#define XILINX_LLTEMAC_FIFO_BASEADDR \
			XPAR_LLTEMAC_0_LLINK_CONNECTED_BASEADDR
	#endif
	#define CONFIG_XILINX_LL_TEMAC	1
	#define CONFIG_SYS_ENET
#endif

#undef ET_DEBUG

/* gpio */
#ifdef XPAR_GPIO_0_BASEADDR
	#define	CONFIG_SYS_GPIO_0		1
	#define	CONFIG_SYS_GPIO_0_ADDR		XILINX_GPIO_BASEADDR
#endif

/* interrupt controller */
#ifdef XPAR_INTC_0_BASEADDR
	#define	CONFIG_SYS_INTC_0		1
	#define	CONFIG_SYS_INTC_0_ADDR		XPAR_INTC_0_BASEADDR
	#define	CONFIG_SYS_INTC_0_NUM		32
#endif

/* timer */
#ifdef XPAR_TMRCTR_0_BASEADDR
	#ifdef XPAR_INTC_0_TMRCTR_0_VEC_ID
		#define	CONFIG_SYS_TIMER_0		1
		#define	CONFIG_SYS_TIMER_0_ADDR	XPAR_TMRCTR_0_BASEADDR
		#define	CONFIG_SYS_TIMER_0_IRQ	XPAR_INTC_0_TMRCTR_0_VEC_ID
		#define	FREQUENCE		XPAR_PROC_BUS_0_FREQ_HZ
		#define	CONFIG_SYS_TIMER_0_PRELOAD	( FREQUENCE/1000 )
	#endif
#elif XPAR_PROC_BUS_0_FREQ_HZ
	#define	CONFIG_XILINX_CLOCK_FREQ	XPAR_PROC_BUS_0_FREQ_HZ
#else
	#error BAD CLOCK FREQ
#endif
/* FSL */
/* #define	CONFIG_SYS_FSL_2 */
/* #define	FSL_INTR_2	1 */

/*
 * memory layout - Example
 * TEXT_BASE = 0x1200_0000;
 * CONFIG_SYS_SRAM_BASE = 0x1000_0000;
 * CONFIG_SYS_SRAM_SIZE = 0x0400_0000;
 *
 * CONFIG_SYS_GBL_DATA_OFFSET = 0x1000_0000 + 0x0400_0000 - 0x1000 = 0x13FF_F000
 * CONFIG_SYS_MONITOR_BASE = 0x13FF_F000 - 0x40000 = 0x13FB_F000
 * CONFIG_SYS_MALLOC_BASE = 0x13FB_F000 - 0x40000 = 0x13F7_F000
 *
 * 0x1000_0000	CONFIG_SYS_SDRAM_BASE
 *					FREE
 * 0x1200_0000	TEXT_BASE
 *		U-BOOT code
 * 0x1202_0000
 *					FREE
 *
 *					STACK
 * 0x13F7_F000	CONFIG_SYS_MALLOC_BASE
 *					MALLOC_AREA	256kB	Alloc
 * 0x11FB_F000	CONFIG_SYS_MONITOR_BASE
 *					MONITOR_CODE	256kB	Env
 * 0x13FF_F000	CONFIG_SYS_GBL_DATA_OFFSET
 *					GLOBAL_DATA	4kB	bd, gd
 * 0x1400_0000	CONFIG_SYS_SDRAM_BASE + CONFIG_SYS_SDRAM_SIZE
 */

/* ddr sdram - main memory */
#ifdef XPAR_DDR2_SDRAM_MPMC_BASEADDR
	#define	CONFIG_SYS_SDRAM_BASE		XPAR_DDR2_SDRAM_MPMC_BASEADDR
	#define	CONFIG_SYS_SDRAM_SIZE		(XPAR_DDR2_SDRAM_MPMC_HIGHADDR - \
					 XPAR_DDR2_SDRAM_MPMC_BASEADDR + 1)
#elif  XPAR_DDR3_SDRAM_MPMC_BASEADDR
	#define	CONFIG_SYS_SDRAM_BASE		XPAR_DDR3_SDRAM_MPMC_BASEADDR
	#define	CONFIG_SYS_SDRAM_SIZE		(XPAR_DDR3_SDRAM_MPMC_HIGHADDR - \
					 XPAR_DDR3_SDRAM_MPMC_BASEADDR + 1)
#elif  XPAR_MPMC_0_MPMC_BASEADDR
	#define	CONFIG_SYS_SDRAM_BASE		XPAR_MPMC_0_MPMC_BASEADDR
	#define	CONFIG_SYS_SDRAM_SIZE		(XPAR_MPMC_0_MPMC_HIGHADDR - \
					 XPAR_MPMC_0_MPMC_BASEADDR + 1)
#else
	#error "DDR is not included in the system"
#endif

#define XILINX_RAM_START		CONFIG_SYS_SDRAM_BASE
#define XILINX_RAM_SIZE			CONFIG_SYS_SDRAM_SIZE

#define	CONFIG_SYS_MEMTEST_START	CONFIG_SYS_SDRAM_BASE
#define	CONFIG_SYS_MEMTEST_END		(CONFIG_SYS_SDRAM_BASE + 0x1000)

/* global pointer */
#define	CONFIG_SYS_GBL_DATA_SIZE	128 /* size of global data */
/* start of global data */
#define	CONFIG_SYS_GBL_DATA_OFFSET	(CONFIG_SYS_SDRAM_BASE + CONFIG_SYS_SDRAM_SIZE - CONFIG_SYS_GBL_DATA_SIZE)

/* monitor code */
#define	SIZE				0x40000
#define	CONFIG_SYS_MONITOR_LEN		(SIZE - CONFIG_SYS_GBL_DATA_SIZE)
#define	CONFIG_SYS_MONITOR_BASE		(CONFIG_SYS_GBL_DATA_OFFSET - CONFIG_SYS_MONITOR_LEN)
#define	CONFIG_SYS_MONITOR_END		(CONFIG_SYS_MONITOR_BASE + CONFIG_SYS_MONITOR_LEN)
#define	CONFIG_SYS_MALLOC_LEN		SIZE
#define	CONFIG_SYS_MALLOC_BASE		(CONFIG_SYS_MONITOR_BASE - CONFIG_SYS_MALLOC_LEN)

/* stack */
#define	CONFIG_SYS_INIT_SP_OFFSET	CONFIG_SYS_MALLOC_BASE

/*#define	RAMENV */

#ifdef FLASH
	#define	CONFIG_SYS_FLASH_BASE		XPAR_FLASH_MEM0_BASEADDR
	#define	CONFIG_SYS_FLASH_SIZE		(XPAR_FLASH_MEM0_HIGHADDR - XPAR_FLASH_MEM0_BASEADDR + 1)
	#define	CONFIG_SYS_FLASH_CFI		1
	#define	CONFIG_FLASH_CFI_DRIVER		1
	#define	CONFIG_SYS_FLASH_EMPTY_INFO	1	/* ?empty sector */
	#define	CONFIG_SYS_MAX_FLASH_BANKS	1	/* max number of memory banks */
	#define	CONFIG_SYS_MAX_FLASH_SECT	512	/* max number of sectors on one chip */
	#define	CONFIG_SYS_FLASH_PROTECTION		/* hardware flash protection */

	#ifdef	RAMENV
		#define	CONFIG_ENV_IS_NOWHERE	1
		#define	CONFIG_ENV_SIZE		0x1000
		#define	CONFIG_ENV_ADDR		(CONFIG_SYS_MONITOR_BASE - CONFIG_ENV_SIZE)

	#else	/* !RAMENV */
		#define	CONFIG_ENV_IS_IN_FLASH	1
		#define	CONFIG_ENV_SECT_SIZE	0x20000	/* 128K(one sector) for env */
		#define	CONFIG_ENV_ADDR		(CONFIG_SYS_FLASH_BASE + (2 * CONFIG_ENV_SECT_SIZE))
		#define	CONFIG_ENV_SIZE		0x20000
	#endif /* !RAMBOOT */
#else /* !FLASH */
	/* ENV in RAM */
	#define	CONFIG_SYS_NO_FLASH	1
	#define	CONFIG_ENV_IS_NOWHERE	1
	#define	CONFIG_ENV_SIZE		0x1000
	#define	CONFIG_ENV_ADDR		(CONFIG_SYS_MONITOR_BASE - CONFIG_ENV_SIZE)
	#define	CONFIG_SYS_FLASH_PROTECTION		/* hardware flash protection */
#endif /* !FLASH */

/* system ace */
#ifdef XPAR_SYSACE_0_BASEADDR
	#define	CONFIG_SYSTEMACE
	/* #define DEBUG_SYSTEMACE */
	#define	SYSTEMACE_CONFIG_FPGA
	#define	CONFIG_SYS_SYSTEMACE_BASE	XPAR_SYSACE_0_BASEADDR
	#define	CONFIG_SYS_SYSTEMACE_WIDTH	XPAR_SYSACE_0_MEM_WIDTH
	#define	CONFIG_DOS_PARTITION
#endif

/* Data Cache */
#ifdef XPAR_MICROBLAZE_0_USE_DCACHE
	#define CONFIG_DCACHE
#else
	#undef CONFIG_DCACHE
#endif

/* Instruction Cache */
#ifdef XPAR_MICROBLAZE_0_USE_ICACHE
	#define CONFIG_ICACHE
#else
	#undef CONFIG_ICACHE
#endif

/*
 * BOOTP options
 */
#define CONFIG_BOOTP_BOOTFILESIZE
#define CONFIG_BOOTP_BOOTPATH
#define CONFIG_BOOTP_GATEWAY
#define CONFIG_BOOTP_HOSTNAME

/*
 * Command line configuration.
 */
#include <config_cmd_default.h>

#define CONFIG_CMD_ASKENV
#define CONFIG_CMD_IRQ
//#define CONFIG_CMD_MFSL
#define CONFIG_CMD_ECHO

#if defined(CONFIG_DCACHE) || defined(CONFIG_ICACHE)
	#define CONFIG_CMD_CACHE
#else
	#undef CONFIG_CMD_CACHE
#endif

#ifndef CONFIG_SYS_ENET
	#undef CONFIG_CMD_NET
	#undef CONFIG_NET_MULTI
#else
	#define CONFIG_CMD_PING
	#define CONFIG_NET_MULTI
#endif

#if defined(CONFIG_SYSTEMACE)
	#define CONFIG_CMD_EXT2
	#define CONFIG_CMD_FAT
#endif

#if defined(FLASH)
	#define CONFIG_CMD_ECHO
	#define CONFIG_CMD_FLASH
	#define CONFIG_CMD_IMLS
	#define CONFIG_CMD_JFFS2

	#if !defined(RAMENV)
		#define CONFIG_CMD_SAVEENV
		#define CONFIG_CMD_SAVES
	#endif
#else
	#undef CONFIG_CMD_IMLS
	#undef CONFIG_CMD_FLASH
	#undef CONFIG_CMD_JFFS2
#endif

#if defined(CONFIG_CMD_JFFS2)
/* JFFS2 partitions */
#define CONFIG_CMD_MTDPARTS	/* mtdparts command line support */
#define CONFIG_MTD_DEVICE	/* needed for mtdparts commands */
#define CONFIG_FLASH_CFI_MTD
#define MTDIDS_DEFAULT		"nor0=ml401-0"

/* default mtd partition table */
#define MTDPARTS_DEFAULT	"mtdparts=ml401-0:256k(u-boot),"\
				"256k(env),3m(kernel),1m(romfs),"\
				"1m(cramfs),-(jffs2)"
#endif

/* Miscellaneous configurable options */
#define	CONFIG_SYS_PROMPT	"U-Boot> "
#define	CONFIG_SYS_CBSIZE	512	/* size of console buffer */
#define	CONFIG_SYS_PBSIZE	(CONFIG_SYS_CBSIZE + sizeof(CONFIG_SYS_PROMPT) + 16) /* print buffer size */
#define	CONFIG_SYS_MAXARGS	15	/* max number of command args */
#define	CONFIG_SYS_LONGHELP
#define	CONFIG_SYS_LOAD_ADDR	XILINX_RAM_START /* default load address */

#define	CONFIG_BOOTDELAY	-1	/* -1 disables auto-boot */
#define	CONFIG_BOOTARGS		"root=romfs"
#define CONFIG_HOSTNAME		microblaze-generic
#define	CONFIG_BOOTCOMMAND	"base 0;tftp 11000000 image.img;bootm"
#define	CONFIG_IPADDR		192.168.0.3
#define	CONFIG_SERVERIP		192.168.0.5
#define	CONFIG_GATEWAYIP	192.168.0.1
#define	CONFIG_ETHADDR		00:E0:0C:00:00:FD

/* architecture dependent code */
#define	CONFIG_SYS_USR_EXCEP	/* user exception */
#define CONFIG_SYS_HZ	1000

#define	CONFIG_PREBOOT		"echo U-BOOT for $(hostname);setenv preboot;echo"

#define	CONFIG_EXTRA_ENV_SETTINGS	"unlock=yes\0" /* hardware flash protection */\
					"nor0=ml401-0\0"\
					"mtdparts=mtdparts=ml401-0:"\
					"256k(u-boot),256k(env),3m(kernel),"\
					"1m(romfs),1m(cramfs),-(jffs2)\0"

#define CONFIG_CMDLINE_EDITING

/* Use the HUSH parser */
#define CONFIG_SYS_HUSH_PARSER
#ifdef  CONFIG_SYS_HUSH_PARSER
#define CONFIG_SYS_PROMPT_HUSH_PS2 "> "
#endif

/* Flat device tree support */
#define CONFIG_OF_LIBFDT
#define CONFIG_SYS_BOOTMAPSZ	(8 << 20)       /* Initial Memory map for Linux */
#define CONFIG_LMB

#endif	/* __CONFIG_H */
