/*
 * (C) Copyright 2012 Michal Simek <monstr@monstr.eu>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <fdtdec.h>
#include <fpga.h>
#include <mmc.h>
#include <zynqpl.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>
#include <spi.h>
#include <spi_flash.h>
#ifdef CONFIG_SPL_BUILD
#include <spl.h>
#endif
#include <image_table.h>

#define REG_REBOOT_STATUS 0xF8000258U

DECLARE_GLOBAL_DATA_PTR;

#if (defined(CONFIG_FPGA) && !defined(CONFIG_SPL_BUILD)) || \
    (defined(CONFIG_SPL_FPGA_SUPPORT) && defined(CONFIG_SPL_BUILD))
static xilinx_desc fpga;

/* It can be done differently */
static xilinx_desc fpga010 = XILINX_XC7Z010_DESC(0x10);
static xilinx_desc fpga015 = XILINX_XC7Z015_DESC(0x15);
static xilinx_desc fpga020 = XILINX_XC7Z020_DESC(0x20);
static xilinx_desc fpga030 = XILINX_XC7Z030_DESC(0x30);
static xilinx_desc fpga035 = XILINX_XC7Z035_DESC(0x35);
static xilinx_desc fpga045 = XILINX_XC7Z045_DESC(0x45);
static xilinx_desc fpga100 = XILINX_XC7Z100_DESC(0x100);
#endif

#ifdef CONFIG_IMAGE_TABLE_BOOT
static int image_descriptor_get(struct spi_flash *flash, uint32_t image_type,
                                image_descriptor_t *image_descriptor)
{
  int err = 0;

  /* Image table index is stored in the lower two bits
   * of REBOOT_STATUS by the loader
   */
  uint32_t image_table_index = readl(REG_REBOOT_STATUS) & 0x3;
  const uint32_t image_set_offsets[] = CONFIG_IMAGE_SET_OFFSETS;
  uint32_t image_set_offset = image_set_offsets[image_table_index];

  /* Load image set from QSPI flash into RAM */
  image_set_t image_set;
  err = spi_flash_read(flash, image_set_offset, sizeof(image_set_t),
                       (void *)&image_set);
  if (err) {
    puts("Failed to read image set\n");
    return err;
  }

  err = image_set_verify(&image_set);
  if (err) {
    puts("Image set verification failed\n");
    return err;
  }

  const image_descriptor_t *d;
  err = image_set_find_descriptor(&image_set, image_type, &d);
  if (err) {
    puts("Failed to find image descriptor\n");
    return err;
  }

  *image_descriptor = *d;
  return 0;
}

static int image_table_env_setup(void)
{
  int err;

  struct spi_flash *flash;
  flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
                          CONFIG_SF_DEFAULT_CS,
                          CONFIG_SF_DEFAULT_SPEED,
                          CONFIG_SF_DEFAULT_MODE);
  if (!flash) {
    puts("SPI probe failed\n");
    return -ENODEV;
  }

  image_descriptor_t image_descriptor;
  err = image_descriptor_get(flash, IMAGE_TYPE_LINUX, &image_descriptor);
  if (err) {
    return err;
  }

  setenv_hex("img_tbl_kernel_load_address", image_descriptor.load_address);
  setenv_hex("img_tbl_kernel_flash_offset", image_descriptor.data_offset);
  setenv_hex("img_tbl_kernel_size",         image_descriptor.data_length);
  return 0;
}
#endif

int board_init(void)
{
#if defined(CONFIG_ENV_IS_IN_EEPROM) && !defined(CONFIG_SPL_BUILD)
	unsigned char eepromsel = CONFIG_SYS_I2C_MUX_EEPROM_SEL;
#endif
#if (defined(CONFIG_FPGA) && !defined(CONFIG_SPL_BUILD)) || \
    (defined(CONFIG_SPL_FPGA_SUPPORT) && defined(CONFIG_SPL_BUILD))
	u32 idcode;

	idcode = zynq_slcr_get_idcode();

	switch (idcode) {
	case XILINX_ZYNQ_7010:
		fpga = fpga010;
		break;
	case XILINX_ZYNQ_7015:
		fpga = fpga015;
		break;
	case XILINX_ZYNQ_7020:
		fpga = fpga020;
		break;
	case XILINX_ZYNQ_7030:
		fpga = fpga030;
		break;
	case XILINX_ZYNQ_7035:
		fpga = fpga035;
		break;
	case XILINX_ZYNQ_7045:
		fpga = fpga045;
		break;
	case XILINX_ZYNQ_7100:
		fpga = fpga100;
		break;
	}
#endif

#if (defined(CONFIG_FPGA) && !defined(CONFIG_SPL_BUILD)) || \
    (defined(CONFIG_SPL_FPGA_SUPPORT) && defined(CONFIG_SPL_BUILD))
	fpga_init();
	fpga_add(fpga_xilinx, &fpga);
#endif
#if defined(CONFIG_ENV_IS_IN_EEPROM) && !defined(CONFIG_SPL_BUILD)
	if (eeprom_write(CONFIG_SYS_I2C_MUX_ADDR, 0, &eepromsel, 1))
		puts("I2C:EEPROM selection failed\n");
#endif
	return 0;
}

int board_late_init(void)
{
	switch ((zynq_slcr_get_boot_mode()) & ZYNQ_BM_MASK) {
	case ZYNQ_BM_QSPI:
		setenv("modeboot", "qspiboot");
		break;
	case ZYNQ_BM_NAND:
		setenv("modeboot", "nandboot");
		break;
	case ZYNQ_BM_NOR:
		setenv("modeboot", "norboot");
		break;
	case ZYNQ_BM_SD:
		setenv("modeboot", "sdboot");
		break;
	case ZYNQ_BM_JTAG:
		setenv("modeboot", "jtagboot");
		break;
	default:
		setenv("modeboot", "");
		break;
	}

#ifdef CONFIG_IMAGE_TABLE_BOOT
	image_table_env_setup();
#endif

	return 0;
}

#ifdef CONFIG_SPL_BUILD
#ifdef CONFIG_SPL_BOARD_LOAD_IMAGE
#ifdef CONFIG_IMAGE_TABLE_BOOT

void board_boot_order(u32 *spl_boot_list)
{
  spl_boot_list[0] = BOOT_DEVICE_BOARD;
}

void spl_board_announce_boot_device(void)
{
  puts("image table");
}

int spl_board_load_image(void)
{
  int err;

  struct spi_flash *flash;
  flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
                          CONFIG_SF_DEFAULT_CS,
                          CONFIG_SF_DEFAULT_SPEED,
                          CONFIG_SF_DEFAULT_MODE);
  if (!flash) {
    puts("SPI probe failed\n");
    return -ENODEV;
  }

  image_descriptor_t image_descriptor;
  err = image_descriptor_get(flash, IMAGE_TYPE_UBOOT, &image_descriptor);
  if (err) {
    return err;
  }

  u32 uboot_offset = image_descriptor.data_offset;

  /*
   * Load U-Boot image from SPI flash into RAM
   */
  struct image_header header;

  /* Load U-Boot, mkimage header is 64 bytes. */
  err = spi_flash_read(flash, uboot_offset, 0x40, (void *)&header);
  if (err) {
    puts("Failed to read U-Boot image header\n");
    return err;
  }

  spl_parse_image_header(&header);
  err = spi_flash_read(flash, uboot_offset,
                       spl_image.size, (void *)spl_image.load_addr);
  if (err) {
    puts("Failed to read U-Boot image\n");
    return err;
  }

  return err;
}

#endif
#endif
#endif

#ifdef CONFIG_DISPLAY_BOARDINFO
int checkboard(void)
{
	puts("Board: Xilinx Zynq\n");
	return 0;
}
#endif

int dram_init(void)
{
#if CONFIG_IS_ENABLED(OF_CONTROL)
	int node;
	fdt_addr_t addr;
	fdt_size_t size;
	const void *blob = gd->fdt_blob;

	node = fdt_node_offset_by_prop_value(blob, -1, "device_type",
					     "memory", 7);
	if (node == -FDT_ERR_NOTFOUND) {
		debug("ZYNQ DRAM: Can't get memory node\n");
		return -1;
	}
	addr = fdtdec_get_addr_size(blob, node, "reg", &size);
	if (addr == FDT_ADDR_T_NONE || size == 0) {
		debug("ZYNQ DRAM: Can't get base address or size\n");
		return -1;
	}
	gd->ram_size = size;
#else
	gd->ram_size = CONFIG_SYS_SDRAM_SIZE;
#endif
	zynq_ddrc_init();

	return 0;
}
