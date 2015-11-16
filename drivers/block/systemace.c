/*
 * Copyright (c) 2004 Picture Elements, Inc.
 *    Stephen Williams (XXXXXXXXXXXXXXXX)
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * The Xilinx SystemACE chip support is activated by defining
 * CONFIG_SYSTEMACE to turn on support, and CONFIG_SYS_SYSTEMACE_BASE
 * to set the base address of the device. This code currently
 * assumes that the chip is connected via a byte-wide bus.
 *
 * The CONFIG_SYSTEMACE also adds to fat support the device class
 * "ace" that allows the user to execute "fatls ace 0" and the
 * like. This works by making the systemace_get_dev function
 * available to cmd_fat.c:get_dev and filling in a block device
 * description that has all the bits needed for FAT support to
 * read sectors.
 *
 * According to Xilinx technical support, before accessing the
 * SystemACE CF you need to set the following control bits:
 *      FORCECFGMODE : 1
 *      CFGMODE : 0
 *      CFGSTART : 0
 */

#include <common.h>
#include <command.h>
#include <systemace.h>
#include <part.h>
#include <asm/io.h>

#define SECTOR_SIZE 512

/*
 * Definitions of the SystemACE registers
 */

#define ACE_BUS_MODE_REG        0x00
#define ACE_BUS_MODE_8          0x00000000
#define ACE_BUS_MODE_16         0x00000101

#define ACE_STATUS_REG          0x04
#define ACE_STATUS_CFGLOCK      0x00000001
#define ACE_STATUS_MPULOCK      0x00000002
#define ACE_STATUS_CFGERROR     0x00000004
#define ACE_STATUS_CFCERROR     0x00000008
#define ACE_STATUS_CFDETECT     0x00000010
#define ACE_STATUS_DATABUFRDY   0x00000020
#define ACE_STATUS_DATABUFMODE  0x00000040
#define ACE_STATUS_CFGDONE      0x00000080
#define ACE_STATUS_RDYFORCFCMD  0x00000100
#define ACE_STATUS_CFGMODEPIN   0x00000200
#define ACE_STATUS_CFGADDR_MASK 0x0000e000

#define ACE_ERROR_REG           0x08

#define ACE_CFGLBA_REG          0x0c

#define ACE_MPULA_REG           0x10

#define ACE_SECCNTCMD_REG       0x14
#define ACE_SECCNTCMD_RESET     0x0100
#define ACE_SECCNTCMD_READ      0x0300

#define ACE_CTRL_REG            0x18
#define ACE_CTRL_FORCELOCKREQ   0x00000001
#define ACE_CTRL_LOCKREQ        0x00000002
#define ACE_CTRL_FORCECFGADDR   0x00000004
#define ACE_CTRL_FORCECFGMODE   0x00000008
#define ACE_CTRL_CFGMODE        0x00000010
#define ACE_CTRL_CFGSTART       0x00000020
#define ACE_CTRL_CFGSEL         0x00000040
#define ACE_CTRL_CFGRESET       0x00000080
#define ACE_CTRL_DATABUFRDYIRQ  0x00000100
#define ACE_CTRL_ERRORIRQ       0x00000200
#define ACE_CTRL_CFGDONEIRQ     0x00000400
#define ACE_CTRL_RESETIRQ       0x00000800
#define ACE_CTRL_CFGPROG        0x00001000
#define ACE_CTRL_CFGADDR_MASK   0x0000e000

#define ACE_DATA_BUF_REG        0x40

/*
 * The ace_read<width> and write<width> functions read/write 16/32bit words,
 * but the  * offset value is the BYTE offset as most used in the Xilinx
 * datasheet for the SystemACE chip. Since the SystemACE always operates in
 * little-endian mode, we do not need to distinguish between the endianess.
 * The CONFIG_SYS_SYSTEMACE_BASE is defined
 * to be the base address for the chip, usually in the local
 * peripheral bus.
 */

static u32 base = CONFIG_SYS_SYSTEMACE_BASE;
static u32 width = CONFIG_SYS_SYSTEMACE_WIDTH;

static void ace_write16(u16 val, unsigned off)
{
	if (width == 8) {
		writeb(val, base + off);
		writeb(val >> 8, base + off + 1);
	} else {
		out16(base + off, val);
	}
}

static void ace_write32(u32 val, unsigned off)
{
	if (width == 8) {
		writeb(val, base + off);
		writeb(val >> 8, base + off + 1);
		writeb(val >> 16, base + off + 2);
		writeb(val >> 24, base + off + 3);
	} else {
		out16(base + off, val);
		out16(base + off + 2, val >> 16);
	}
}

static u16 ace_read16(unsigned off)
{
	if (width == 8) {
		return readb(base + off) | (readb(base + off + 1) << 8);
	} else {
		return in16(base + off);
	}
}

static u32 ace_read32(unsigned off)
{
	if (width == 8) {
		return readb(base + off) | (readb(base + off + 1) << 8)
		        | (readb(base + off + 2) << 16) | (readb(base + off + 3) << 24);
	} else {
		return in16(base + off) | (in16(base + off + 2) << 16);
	}
}

static void ace_or16(u16 val, unsigned off)
{
	val = ace_read16(off) | val;
	ace_write16(val, off);
}

static void ace_or32(u32 val, unsigned off)
{
	val = ace_read32(off) | val;
	ace_write32(val, off);
}

static void ace_and16(u16 val, unsigned off)
{
	val = ace_read16(off) & val;
	ace_write16(val, off);
}

static void ace_and32(u32 val, unsigned off)
{
	val = ace_read32(off) & val;
	ace_write32(val, off);
}

static unsigned long systemace_read(int dev, unsigned long start,
					lbaint_t blkcnt, void *buffer);

static block_dev_desc_t systemace_dev = { 0 };

static int get_cf_lock(void)
{
	int retry = 10;

	ace_write32(ACE_CTRL_FORCECFGMODE, ACE_CTRL_REG);
	/* CONTROLREG = LOCKREG */
	ace_or32(ACE_CTRL_LOCKREQ, ACE_CTRL_REG);

	/* Wait for MPULOCK in STATUSREG[15:0] */
	while (!(ace_read32(ACE_STATUS_REG) & ACE_STATUS_MPULOCK)) {
		if (retry < 0)
			return -1;

		udelay(100000);
		retry -= 1;
	}

	return 0;
}

static void release_cf_lock(void)
{
	ace_and32(~ACE_CTRL_LOCKREQ, ACE_CTRL_REG);
}

#ifdef CONFIG_PARTITIONS
block_dev_desc_t *systemace_get_dev(int dev)
{
	/* The first time through this, the systemace_dev object is
	   not yet initialized. In that case, fill it in. */
	if (systemace_dev.blksz == 0) {
		systemace_dev.if_type = IF_TYPE_UNKNOWN;
		systemace_dev.dev = 0;
		systemace_dev.part_type = PART_TYPE_UNKNOWN;
		systemace_dev.type = DEV_TYPE_HARDDISK;
		systemace_dev.blksz = 512;
		systemace_dev.log2blksz = LOG2(systemace_dev.blksz);
		systemace_dev.removable = 1;
		systemace_dev.block_read = systemace_read;

		/*
		 * Ensure the correct bus mode (8/16 bits) gets enabled
		 */
		if (width == 8) {
			ace_write32(ACE_BUS_MODE_8, ACE_BUS_MODE_REG);
		} else {
			ace_write32(ACE_BUS_MODE_16, ACE_BUS_MODE_REG);
		}

		init_part(&systemace_dev);

	}

	return &systemace_dev;
}
#endif

/*
 * This function is called (by dereferencing the block_read pointer in
 * the dev_desc) to read blocks of data. The return value is the
 * number of blocks read. A zero return indicates an error.
 */
static unsigned long systemace_read(int dev, unsigned long start,
					lbaint_t blkcnt, void *buffer)
{
	int retry;
	unsigned blk_countdown;
	unsigned char *dp = buffer;
	unsigned val;

	if (get_cf_lock() < 0) {
		unsigned status = ace_read32(ACE_STATUS_REG);

		/* If CFDETECT is false, card is missing. */
		if (!(status & ACE_STATUS_CFDETECT)) {
			printf("** CompactFlash card not present. **\n");
			return 0;
		}

		printf("**** ACE locked away from me (STATUSREG=%04x)\n",
		       status);
		return 0;
	}

#ifdef DEBUG_SYSTEMACE
	printf("... systemace read %lu sectors at %lu\n", blkcnt, start);
#endif

	retry = 2000;
	for (;;) {
		val = ace_read32(ACE_STATUS_REG);

		/* If CFDETECT is false, card is missing. */
		if (!(val & ACE_STATUS_CFDETECT)) {
			printf("**** ACE CompactFlash not found.\n");
			release_cf_lock();
			return 0;
		}

		/* If RDYFORCMD, then we are ready to go. */
		if (val & ACE_STATUS_RDYFORCFCMD)
			break;

		if (retry < 0) {
			printf("**** SystemACE not ready.\n");
			release_cf_lock();
			return 0;
		}

		udelay(1000);
		retry -= 1;
	}

	/* The SystemACE can only transfer 256 sectors at a time, so
	   limit the current chunk of sectors. The blk_countdown
	   variable is the number of sectors left to transfer. */

	blk_countdown = blkcnt;
	while (blk_countdown > 0) {
		unsigned trans = blk_countdown;
		unsigned bytes = 0;

		if (trans > 256)
			trans = 256;

#ifdef DEBUG_SYSTEMACE
		printf("... transfer %lu sector in a chunk\n", trans);
#endif

		/* Write LBA block address */
		ace_write32(start & 0x0fffffff, ACE_MPULA_REG);

		/* NOTE: in the Write Sector count below, a count of 0
		   causes a transfer of 256, so &0xff gives the right
		   value for whatever transfer count we want. */

		/* Write sector count | ReadMemCardData. */
		ace_write16((trans & 0xff) | ACE_SECCNTCMD_READ, ACE_SECCNTCMD_REG);

/*
 * For FPGA configuration via SystemACE is reset unacceptable
 * CFGDONE bit in STATUSREG is not set to 1.
 */
#ifndef SYSTEMACE_CONFIG_FPGA
		/* Reset the configruation controller */
		ace_or32(ACE_CTRL_CFGRESET, ACE_CTRL_REG);
#endif

		for (bytes = 0; bytes < trans * SECTOR_SIZE; bytes += 2) {
			/* Wait for buffer to become ready. */
			while (!(ace_read32(ACE_STATUS_REG) & ACE_STATUS_DATABUFRDY)) {
				udelay(100);
			}

			unsigned short val = ace_read16(ACE_DATA_BUF_REG);

			*dp++ = val & 0xff;
			*dp++ = (val >> 8) & 0xff;
		}

		/* Clear the configruation controller reset */
		ace_and32(~ACE_CTRL_CFGRESET, ACE_CTRL_REG);

		/* Count the blocks we transfer this time. */
		start += trans;
		blk_countdown -= trans;
	}

	release_cf_lock();

	return blkcnt;
}
