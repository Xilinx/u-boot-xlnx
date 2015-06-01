/*
 * (C) Copyright 2014 ApisSys SAS
 *
 * Configuration for AV108 Zynq FMC, XMC Carrier board
 * See zynq-common.h for Zynq common configs
 *
 * Author: Mikael Delacour
 * 
 * Date of creation: 07/2014
 * 
 * SPDX-License-Identifier:	GPL-2.0+
 */


#ifndef __CONFIG_ZYNQ_AV108_H
#define __CONFIG_ZYNQ_AV108_H

#define CONFIG_SYS_SDRAM_SIZE		(1024 * 1024 * 1024)

#define CONFIG_ZYNQ_SERIAL_UART0

#define CONFIG_ZYNQ_GEM0
#define CONFIG_ZYNQ_GEM_PHY_ADDR0	0

#define CONFIG_SYS_NO_FLASH
#define CONFIG_NAND_ZYNQ


#define CONFIG_ZYNQ_USB

/*#define CONFIG_NAND_ZYNQ*/

#define CONFIG_ZYNQ_BOOT_FREEBSD

/*#define CONFIG_DEFAULT_DEVICE_TREE	zynq-zed*/

/*
#include <video_fb.h>
#define CONFIG_CFB_CONSOLE
#define VIDEO_VISIBLE_COLS 	1920
#define VIDEO_VISIBLE_ROWS	1080
#define VIDEO_PIXEL_SIZE	4
#define VIDEO_DATA_FORMAT	GDF_32BIT_X888RGB
#define VIDEO_FB_ADRS		0x5000000
*/

#define CONFIG_ENV_OFFSET	0x6D00000




#include <configs/zynq-common.h>



/* Default environment */
#define CONFIG_EXTRA_ENV_SETTINGS	\
	"ethaddr=00:0a:35:00:01:22\0"	\
	"kernel_image=uImage\0"	\
	"ramdisk_image=uramdisk.image.gz\0"	\
	"devicetree_image=devicetree.dtb\0"	\
	"bitstream_image=system.bit.bin\0"	\
	"boot_image=BOOT.bin\0"	\
	"loadbit_addr=0x100000\0"	\
	"loadbootenv_addr=0x2000000\0" \
	"kernel_size=0x500000\0"	\
	"devicetree_size=0x20000\0"	\
	"ramdisk_size=0x5E0000\0"	\
	"boot_size=0xF00000\0"	\
	"fdt_high=0x20000000\0"	\
	"initrd_high=0x20000000\0"	\
	"bootenv=uEnv.txt\0" \
	"loadbootenv=fatload mmc 0 ${loadbootenv_addr} ${bootenv}\0" \
	"importbootenv=echo Importing environment from SD ...; " \
		"env import -t ${loadbootenv_addr} $filesize\0" \
	"mmc_loadbit_fat=echo Loading bitstream from SD/MMC/eMMC to RAM.. && " \
		"mmcinfo && " \
		"fatload mmc 0 ${loadbit_addr} ${bitstream_image} && " \
		"fpga load 0 ${loadbit_addr} ${filesize}\0" \
	"norboot=echo Copying Linux from NOR flash to RAM... && " \
		"cp.b 0xE2100000 0x3000000 ${kernel_size} && " \
		"cp.b 0xE2600000 0x2A00000 ${devicetree_size} && " \
		"echo Copying ramdisk... && " \
		"cp.b 0xE2620000 0x2000000 ${ramdisk_size} && " \
		"bootm 0x3000000 0x2000000 0x2A00000\0" \
	"qspiboot=echo Copying Linux from QSPI flash to RAM... && " \
		"sf probe 0 0 0 && " \
		"sf read 0x3000000 0x100000 ${kernel_size} && " \
		"sf read 0x2A00000 0x600000 ${devicetree_size} && " \
		"echo Copying ramdisk... && " \
		"sf read 0x2000000 0x620000 ${ramdisk_size} && " \
		"bootm 0x3000000 0x2000000 0x2A00000\0" \
	"uenvboot=" \
		"if run loadbootenv; then " \
			"echo Loaded environment from ${bootenv}; " \
			"run importbootenv; " \
		"fi; " \
		"if test -n $uenvcmd; then " \
			"echo Running uenvcmd ...; " \
			"run uenvcmd; " \
		"fi\0" \
	"sdboot=if mmcinfo; then " \
			"run uenvboot; " \
			"echo Copying Linux from SD to RAM... && " \
			"fatload mmc 0 0x3000000 ${kernel_image} && " \
			"fatload mmc 0 0x2A00000 ${devicetree_image} && " \
			"fatload mmc 0 0x2000000 ${ramdisk_image} && " \
			"bootm 0x3000000 0x2000000 0x2A00000; " \
		"fi\0" \
	"usbboot=if usb start; then " \
			"echo Copying Linux from USB to RAM... && " \
			"fatload usb 0:1 0x10000000 av108.dtb && " \
			"fatload usb 0:1 0x30000000 uimage && " \
			"bootm 0x30000000 - 0x10000000; " \
		"fi\0" \
	"nandboot=echo Copying Linux from NAND flash to RAM... && " \
		"nand read 0x10000000 0x2C00000 0x100000 && " \
		"nand read 0x30000000 0x2D00000 0x4000000 && " \
		"bootm 0x30000000 - 0x10000000\0" \
	"jtagboot=echo TFTPing Linux to RAM... && " \
		"tftpboot 0x3000000 ${kernel_image} && " \
		"tftpboot 0x2A00000 ${devicetree_image} && " \
		"tftpboot 0x2000000 ${ramdisk_image} && " \
		"bootm 0x3000000 0x2000000 0x2A00000\0" \
	"rsa_norboot=echo Copying Image from NOR flash to RAM... && " \
		"cp.b 0xE2100000 0x100000 ${boot_size} && " \
		"zynqrsa 0x100000 && " \
		"bootm 0x3000000 0x2000000 0x2A00000\0" \
	"rsa_nandboot=echo Copying Image from NAND flash to RAM... && " \
		"nand read 0x100000 0x0 ${boot_size} && " \
		"zynqrsa 0x100000 && " \
		"bootm 0x3000000 0x2000000 0x2A00000\0" \
	"rsa_qspiboot=echo Copying Image from QSPI flash to RAM... && " \
		"sf probe 0 0 0 && " \
		"sf read 0x100000 0x0 ${boot_size} && " \
		"zynqrsa 0x100000 && " \
		"bootm 0x3000000 0x2000000 0x2A00000\0" \
	"rsa_sdboot=echo Copying Image from SD to RAM... && " \
		"fatload mmc 0 0x100000 ${boot_image} && " \
		"zynqrsa 0x100000 && " \
		"bootm 0x3000000 0x2000000 0x2A00000\0" \
	"rsa_jtagboot=echo TFTPing Image to RAM... && " \
		"tftpboot 0x100000 ${boot_image} && " \
		"zynqrsa 0x100000 && " \
		"bootm 0x3000000 0x2000000 0x2A00000\0"








/*#undef CONFIG_EXTRA_ENV_SETTINGS*/
/* Default environment */
/*#define CONFIG_EXTRA_ENV_SETTINGS	\
	"kernel_image=uImage\0"	\
	"ramdisk_image=uramdisk.image.gz\0"	\
	"devicetree_image=av108.dtb\0"	\
	"boot_image=BOOT.bin\0"	\
	"usbboot=if usb start; then " \
			"echo Copying Linux from USB to RAM... && " \
			"fatload usb 0:1 0x10000000 av108.dtb" \
			"fatload usb 0:1 0x30000000 uimage" \
			"bootm 0x30000000 - 0x10000000\0" \
		"fi\0" \
	"nandboot=echo Copying Linux from NAND flash to RAM... && " \
		"nand read 0x10000000 0x2C00000 0x100000 && " \
		"nand read 0x30000000 0x2D00000 0x4000000 && " \
		"bootm 0x30000000 - 0x10000000\0"
*/

#undef CONFIG_SYS_PROMPT
#define CONFIG_SYS_PROMPT	"av108-boot> "

#endif /* __CONFIG_ZYNQ_AV108_H */