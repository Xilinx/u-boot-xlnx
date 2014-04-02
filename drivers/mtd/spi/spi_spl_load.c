/*
 * Copyright (C) 2011 OMICRON electronics GmbH
 *
 * based on drivers/mtd/nand/nand_spl_load.c
 *
 * Copyright (C) 2011
 * Heiko Schocher, DENX Software Engineering, hs@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <spi_flash.h>
#include <spl.h>

__weak u32 spl_spi_u_boot_offs(struct spi_flash *flash)
{
    return CONFIG_SYS_SPI_U_BOOT_OFFS;
}

/*
 * The main entry for SPI booting. It's necessary that SDRAM is already
 * configured and available since this code loads the main U-Boot image
 * from SPI into SDRAM and starts it from there.
 */
void spl_spi_load_image(void)
{
	struct spi_flash *flash;
	struct image_header *header;
	u32 offs;

	/*
	 * Load U-Boot image from SPI flash into RAM
	 */

	flash = spi_flash_probe(CONFIG_SPL_SPI_BUS, CONFIG_SPL_SPI_CS,
				CONFIG_SF_DEFAULT_SPEED, SPI_MODE_3);
	if (!flash) {
		puts("SPI probe failed.\n");
		hang();
	}

	/* use CONFIG_SYS_TEXT_BASE as temporary storage area */
	header = (struct image_header *)(CONFIG_SYS_TEXT_BASE);

	/* Load u-boot, mkimage header is 64 bytes. */
	offs = spl_spi_u_boot_offs(flash);
	spi_flash_read(flash, offs, 0x40, (void *)header);
	spl_parse_image_header(header);
	spi_flash_read(flash, offs, spl_image.size, (void *)spl_image.load_addr);
}
