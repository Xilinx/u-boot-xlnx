/*
 * Copyright 2010, PetaLogix.
 * Author: Wendy Liang <wendy.liang@petalogix.com>
 * Licensed under the GPL-2 or later.
 */

//#define DEBUG 1

#include <common.h>
#include <malloc.h>
#include <spi_flash.h>

#include "spi_flash_internal.h"

/* M25Pxx-specific commands */
#define CMD_PLNX_WREN		0x06	/* Write Enable */
#define CMD_PLNX_WRDI		0x04	/* Write Disable */
#define CMD_PLNX_RDSR		0x05	/* Read Status Register */
#define CMD_PLNX_WRSR		0x01	/* Write Status Register */
#define CMD_PLNX_READ		0x03	/* Read Data Bytes */
#define CMD_PLNX_FAST_READ	0x0b	/* Read Data Bytes at Higher Speed */
#define CMD_PLNX_PP		0x02	/* Page Program */
#define CMD_PLNX_BE_4K		0x20	/* Sector (4K) Erase */
#define CMD_PLNX_BE_32K		0x52	/* Sector (4K) Erase */
#define CMD_PLNX_SE		0xd8	/* Sector (64K) Erase */
#define CMD_PLNX_CE		0xc7	/* Chip Erase */
#define CMD_PLNX_DP		0xb9	/* Deep Power-down */
#define CMD_PLNX_RES		0xab	/* Release from DP, and Read Signature */
#define PLNX_ID_W25Q64		0xef4017
#define PLNX_ID_N25Q128		0x20ba18

#define PLNX_SR_WIP		(1 << 0)	/* Write-in-Progress */

struct plnx_spi_flash_params {
	uint32_t	id;
	/* Log2 of page size in power-of-two mode */
	uint8_t		l2_page_size;
	uint16_t	pages_per_sector;
	uint16_t        sectors_per_block;
	uint8_t         nr_blocks;
	uint16_t	nr_sectors;
	uint8_t		sector_erase_op;
	const char	*name;
};

/* spi_flash needs to be first so upper layers can free() it */
struct plnx_spi_flash {
	struct spi_flash flash;
	const struct plnx_spi_flash_params *params;
};

static inline struct plnx_spi_flash *
to_plnx_spi_flash(struct spi_flash *flash)
{
	return container_of(flash, struct plnx_spi_flash, flash);
}

static const struct plnx_spi_flash_params plnx_spi_flash_table[] = {
	{
		.id			= PLNX_ID_W25Q64,
		.l2_page_size		= 8,
		.pages_per_sector	= 16,
		.sectors_per_block	= 16,
		.nr_blocks		= 128,
		.nr_sectors		= 2048,
		.sector_erase_op	= CMD_PLNX_BE_4K,
		.name			= "W25Q64",
	},
	{
		.id			= PLNX_ID_N25Q128,
		.l2_page_size           = 8,
		.pages_per_sector       = 256,
		.sectors_per_block      = 0,
		.nr_blocks              = 0,
		.nr_sectors             = 256,
		.sector_erase_op	= CMD_PLNX_SE,
		.name			= "N25Q128",
	},
};

static int plnx_wait_ready(struct spi_flash *flash, unsigned long timeout)
{
	struct spi_slave *spi = flash->spi;
	unsigned long timebase;
	int ret;
	u8 status;
	u8 cmd = CMD_PLNX_RDSR;

	ret = spi_xfer(spi, 8, &cmd, NULL, SPI_XFER_BEGIN);
	if (ret) {
		printf("SF: Failed to send command %02x: %d\n", cmd, ret);
		return ret;
	}

	timebase = get_timer(0);
	do {
		ret = spi_xfer(spi, 8, NULL, &status, 0);
		if (ret) {
			printf("SF: Failed to get status for cmd %02x: %d\n", cmd, ret);
			return -1;
		}

		if ((status & PLNX_SR_WIP) == 0)
			break;

	} while (get_timer(timebase) < timeout);

	spi_xfer(spi, 0, NULL, NULL, SPI_XFER_END);

	if ((status & PLNX_SR_WIP) == 0)
		return 0;

	debug("SF: Timed out on command %02x: %d\n", cmd, ret);
	/* Timed out */
	return -1;
}

static int plnx_read_fast(struct spi_flash *flash,
		u32 offset, size_t len, void *buf)
{
	struct plnx_spi_flash *stm = to_plnx_spi_flash(flash);
	unsigned long page_addr;
	unsigned long byte_addr;
	unsigned long page_size;
	unsigned int page_shift;
	u8 cmd[5];

	/*
	 * The "extra" space per page is the power-of-two page size
	 * divided by 32.
	 */
	page_shift = stm->params->l2_page_size;
	page_size = (1 << page_shift);
	page_addr = offset / page_size;
	byte_addr = offset % page_size;

	cmd[0] = CMD_READ_ARRAY_FAST;
	cmd[1] = page_addr >> (16 - page_shift);
	cmd[2] = page_addr << (page_shift - 8) | (byte_addr >> 8);
	cmd[3] = byte_addr;
	cmd[4] = 0x00;
	
	debug("%s, expected len: 0x%x\n", __FUNCTION__, len);

	return spi_flash_read_common(flash, cmd, sizeof(cmd), buf, len);
}

static int plnx_write(struct spi_flash *flash,
		u32 offset, size_t len, const void *buf)
{
	struct plnx_spi_flash *stm = to_plnx_spi_flash(flash);
	unsigned long page_addr;
	unsigned long byte_addr;
	unsigned long page_size;
	unsigned int page_shift;
	size_t chunk_len;
	size_t actual;
	int ret;
	u8 cmd[4];

	page_shift = stm->params->l2_page_size;
	page_size = (1 << page_shift);
	page_addr = offset / page_size;
	byte_addr = offset % page_size;

	ret = spi_claim_bus(flash->spi);
	if (ret) {
		printf("SF: Unable to claim SPI bus\n");
		return ret;
	}

	for (actual = 0; actual < len; actual += chunk_len) {
		chunk_len = min(len - actual, page_size - byte_addr);

		cmd[0] = CMD_PLNX_PP;
		cmd[1] = page_addr >> (16 - page_shift);
		cmd[2] = page_addr << (page_shift - 8) | (byte_addr >> 8);
		cmd[3] = byte_addr;
		debug("PP: 0x%p => cmd = { 0x%02x 0x%02x%02x%02x } chunk_len = %d\n",
			buf + actual,
			cmd[0], cmd[1], cmd[2], cmd[3], chunk_len);

		ret = spi_flash_cmd(flash->spi, CMD_PLNX_WREN, NULL, 0);
		if (ret < 0) {
			debug("SF: Enabling Write failed\n");
			goto out;
		}

		ret = spi_flash_cmd_write(flash->spi, cmd, 4,
				buf + actual, chunk_len);
		if (ret < 0) {
			printf("SF: Plnx: Page Program failed\n");
			goto out;
		}

		ret = plnx_wait_ready(flash, SPI_FLASH_PROG_TIMEOUT);
		if (ret < 0) {
			printf("SF: Plnx: page programming timed out\n");
			goto out;
		}

		page_addr++;
		byte_addr = 0;
	}

	debug("SF:Plnx: Successfully programmed %u bytes @ 0x%x\n",
			len, offset);
	ret = 0;

out:
	spi_release_bus(flash->spi);
	return ret;
}

int plnx_erase(struct spi_flash *flash, u32 offset, size_t len)
{
	struct plnx_spi_flash *stm = to_plnx_spi_flash(flash);
	unsigned long sector_size;
	unsigned int  sector_offset;
	size_t actual;
	int ret;
	u8 cmd[4];

	/*
	 * This function currently uses sector erase only.
	 * probably speed things up by using bulk erase
	 * when possible.
	 */
	sector_size = (1 << stm->params->l2_page_size) * stm->params->pages_per_sector;

	if (offset % sector_size || len % sector_size) {
		printf("SF: Erase offset/length not multiple of sector size\n");
		return -1;
	}

	len /= sector_size;
	cmd[0] = stm->params->sector_erase_op;

	ret = spi_claim_bus(flash->spi);
	if (ret) {
		printf("SF: Unable to claim SPI bus\n");
		return ret;
	}
	
	sector_offset = offset;
	for (actual = 0; actual < len; actual++, sector_offset+=sector_size) {
//		plnx_build_address(stm, &cmd[1], offset + actual * sector_size);
		cmd[1] = sector_offset >> 16;
		cmd[2] = sector_offset >> 8;
		cmd[3] = (u8)sector_offset;
		debug("Erase: %02x %02x %02x %02x\n",
				cmd[0], cmd[1], cmd[2], cmd[3]);

		ret = spi_flash_cmd(flash->spi, CMD_PLNX_WREN, NULL, 0);
		if (ret < 0) {
			debug("SF: Enabling Write failed\n");
			goto out;
		}

		ret = spi_flash_cmd_write(flash->spi, cmd, 4, NULL, 0);
		if (ret < 0) {
			printf("SF: Plnx: sector erase failed\n");
			goto out;
		}

		ret = plnx_wait_ready(flash, SPI_FLASH_PAGE_ERASE_TIMEOUT);
		if (ret < 0) {
			printf("SF: Plnx: sector erase timed out\n");
			goto out;
		}
		
	}

	debug("SF: Plnx: Successfully erased %u bytes @ 0x%x\n",
			len * sector_size, offset);
	ret = 0;

out:
	spi_release_bus(flash->spi);
	return ret;
}

struct spi_flash *spi_flash_probe_plnx(struct spi_slave *spi, u8 *idcode)
{
	const struct plnx_spi_flash_params *params;
	unsigned long page_size;
	struct plnx_spi_flash *stm;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(plnx_spi_flash_table); i++) {
		params = &plnx_spi_flash_table[i];
		if (params->id == ((idcode[0] << 16) | (idcode[1] << 8) | idcode[2]))
			break;
	}

	if (i == ARRAY_SIZE(plnx_spi_flash_table)) {
		printf("SF: Unsupported ID %02x%02x%02x\n",
				idcode[0], idcode[1], idcode[2]);
		return NULL;
	}

	stm = malloc(sizeof(struct plnx_spi_flash));
	if (!stm) {
		printf("SF: Failed to allocate memory\n");
		return NULL;
	}

	stm->params = params;
	stm->flash.spi = spi;
	stm->flash.name = params->name;

	/* Assuming power-of-two page size initially. */
	page_size = 1 << params->l2_page_size;

	stm->flash.write = plnx_write;
	stm->flash.erase = plnx_erase;
	stm->flash.read = plnx_read_fast;
	stm->flash.sector_size = page_size * params->pages_per_sector;
	stm->flash.size = page_size * params->pages_per_sector
				* params->nr_sectors;

	debug("SF: Detected %s with page size %u, total %u bytes\n",
			params->name, page_size, stm->flash.size);

	return &stm->flash;
}

