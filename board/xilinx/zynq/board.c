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
#include <factory_data.h>

#define REG_REBOOT_STATUS 0xF8000258U
#define QSPI_LINEAR_START 0xFC000000U
#define FACTORY_DATA_SIZE_MAX 2048

#define MIO_CFG_INPUT_PU  0x1601
#define MIO_CFG_DEFAULT   0x1601
#define MIO_CFG_OUTPUT    0x0600

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

#ifdef CONFIG_FACTORY_DATA
#ifndef CONFIG_TPL_BUILD
static struct {
  uint32_t hardware;
  uint8_t uuid[16];
  uint8_t mac_address[6];
} factory_params;

static void print_hex_string(char *str, const uint8_t *data, uint32_t data_size)
{
  uint32_t i;
  for (i = 0; i < data_size; i++) {
    str += sprintf(str, "%02x", data[data_size - 1 - i]);
  }
}

static int factory_params_read(void)
{
  int err = 0;

  struct spi_flash *flash;
  flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
                          CONFIG_SF_DEFAULT_CS,
                          CONFIG_SF_DEFAULT_SPEED,
                          CONFIG_SF_DEFAULT_MODE);
  if (!flash) {
    puts("SPI probe failed\n");
    return -ENODEV;
  }

  uint8_t factory_data_buff[FACTORY_DATA_SIZE_MAX];
  factory_data_t *factory_data = (factory_data_t *)factory_data_buff;

  /* Load factory data from QSPI flash into RAM */
  err = spi_flash_read(flash, CONFIG_FACTORY_DATA_OFFSET,
                       sizeof(factory_data_buff), (void *)factory_data_buff);
  if (err) {
    puts("Failed to read factory data\n");
    return err;
  }

  /* Verify header */
  if (factory_data_header_verify(factory_data) != 0) {
    puts("Error verifying factory data header\n");
    return -1;
  }

  uint32_t factory_data_body_size = factory_data_body_size_get(factory_data);
  uint32_t factory_data_size = sizeof(factory_data_t) +
                               factory_data_body_size;

  /* Check header + body length */
  if (factory_data_size > sizeof(factory_data_buff)) {
    puts("Factory data is too large\n");
    return -1;
  }

  /* Verify body */
  if (factory_data_body_verify(factory_data) != 0) {
    puts("Error verifying factory data body\n");
    return -1;
  }

  /* Read params */
  if (factory_data_hardware_get(factory_data, &factory_params.hardware) != 0) {
    puts("Error reading hardware parameter from factory data\n");
    return -1;
  }

  if (factory_data_uuid_get(factory_data,
                            factory_params.uuid) != 0) {
    puts("Error reading uuid from factory data\n");
    return -1;
  }

  if (factory_data_mac_address_get(factory_data,
                                   factory_params.mac_address) != 0) {
    puts("Error reading MAC address from factory data\n");
    return -1;
  }

  return err;
}
#endif
#endif

#ifdef CONFIG_IMAGE_TABLE_BOOT
#ifndef CONFIG_TPL_BUILD
static int image_descriptor_get(struct spi_flash *flash, u32 image_type,
                                image_descriptor_t *image_descriptor)
{
  int err = 0;

  /* Image table index is stored in the lower two bits
   * of REBOOT_STATUS by the loader
   */
  u32 image_table_index = readl(REG_REBOOT_STATUS) & 0x3;
  const u32 image_set_offsets[] = CONFIG_IMAGE_SET_OFFSETS;
  u32 image_set_offset = image_set_offsets[image_table_index];

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
  err = image_set_descriptor_find(&image_set, image_type, &d);
  if (err) {
    puts("Failed to find image descriptor\n");
    return err;
  }

  *image_descriptor = *d;
  return err;
}

static int image_table_env_setup(void)
{
  int err = 0;

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

  setenv_hex("img_tbl_kernel_load_address",
             image_descriptor_load_address_get(&image_descriptor));
  setenv_hex("img_tbl_kernel_flash_offset",
             image_descriptor_data_offset_get(&image_descriptor));
  setenv_hex("img_tbl_kernel_size",
             image_descriptor_data_size_get(&image_descriptor));

  /* crc32 verify command requires CRC to be exactly 8 hex digits */
  char buf[16];
  snprintf(buf, sizeof(buf), "%08x",
           image_descriptor_data_crc_get(&image_descriptor));
  setenv("img_tbl_kernel_crc", buf);

  return err;
}
#endif
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
  int err = 0;

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

#ifdef CONFIG_HW_WDT_DIS_MIO
#ifndef CONFIG_SPL_BUILD
  /* Assert HW_WDT_DIS */
  zynq_slcr_unlock();
  writel(MIO_CFG_OUTPUT, &slcr_base->mio_pin[CONFIG_HW_WDT_DIS_MIO]);
  zynq_slcr_lock();
  zynq_gpio_cfg_output(CONFIG_HW_WDT_DIS_MIO);
  zynq_gpio_output_write(CONFIG_HW_WDT_DIS_MIO, 1);
#endif
#endif

#ifdef CONFIG_IMAGE_TABLE_BOOT
#ifndef CONFIG_TPL_BUILD
	err = image_table_env_setup();
  if (err) {
    return err;
  }
#endif
#endif

#ifdef CONFIG_FACTORY_DATA
#ifndef CONFIG_TPL_BUILD
  err = factory_params_read();
  if (err) {
#ifdef CONFIG_FACTORY_DATA_FALLBACK
    puts("*** Warning - using default factory parameters\n");
    memset(&factory_params, 0, sizeof(factory_params));
    err = 0;
#else
    return err;
#endif
  }

  /* Set uuid environment variable */
  char buf[sizeof(factory_params.uuid) * 2 + 1];
  print_hex_string(buf, factory_params.uuid, sizeof(factory_params.uuid));
  setenv("uuid", buf);
#endif
#endif

	return err;
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

#ifdef CONFIG_TPL_BUILD
int tpl_image_set_check(const image_set_t *image_set)
{
  int err = 0;

  /* Verify image set header */
  err = image_set_verify(image_set);
  if (err) {
    return err;
  }

  /* Find SPL image descriptor */
  const image_descriptor_t *image_descriptor;
  err = image_set_descriptor_find(image_set, IMAGE_TYPE_UBOOT_SPL,
                                  &image_descriptor);
  if (err) {
    return err;
  }

  u32 image_data_offset = image_descriptor_data_offset_get(image_descriptor);
  u32 image_data_size = image_descriptor_data_size_get(image_descriptor);
  const u8 *image_data = (const u8 *)(QSPI_LINEAR_START + image_data_offset);

  /* Verify SPL image data CRC */
  u32 computed_data_crc;
  image_descriptor_data_crc_init(&computed_data_crc);
  image_descriptor_data_crc_continue(&computed_data_crc, image_data,
                                     image_data_size);
  if (computed_data_crc != image_descriptor_data_crc_get(image_descriptor)) {
    return -1;
  }

  return 0;
}
#endif

int spl_board_load_image(void)
{
  int err = 0;

#ifdef CONFIG_TPL_BUILD
  /* TPL */
  zynq_slcr_unlock();

  /* Configure RX0, RX1, and TX0 pins as input pullup */
  writel(MIO_CFG_INPUT_PU, &slcr_base->mio_pin[CONFIG_TPL_BOOTSTRAP_RX0_MIO]);
  writel(MIO_CFG_INPUT_PU, &slcr_base->mio_pin[CONFIG_TPL_BOOTSTRAP_RX1_MIO]);
  writel(MIO_CFG_INPUT_PU, &slcr_base->mio_pin[CONFIG_TPL_BOOTSTRAP_TX0_MIO]);

  /* Configure TX1 as output */
  writel(MIO_CFG_OUTPUT, &slcr_base->mio_pin[CONFIG_TPL_BOOTSTRAP_TX1_MIO]);
  zynq_gpio_cfg_output(CONFIG_TPL_BOOTSTRAP_TX1_MIO);

  int drive_count = 4;
  int drive_state = 1;
  bool drive_match = true;
  do {
    /* Drive TX1 */
    zynq_gpio_output_write(CONFIG_TPL_BOOTSTRAP_TX1_MIO, drive_state);

    /* Small delay */
    udelay(10);

    /* Read TX0 */
    if (zynq_gpio_input_read(CONFIG_TPL_BOOTSTRAP_TX0_MIO) != drive_state) {
      drive_match = false;
      break;
    }

    drive_state = (drive_state == 0) ? 1 : 0;
  } while (--drive_count > 0);

  /* Determine boot mode */
  bool failsafe = false;
  bool alt = false;
  if (drive_match) {
    failsafe = (zynq_gpio_input_read(CONFIG_TPL_BOOTSTRAP_RX0_MIO) == 0);
    alt =      (zynq_gpio_input_read(CONFIG_TPL_BOOTSTRAP_RX1_MIO) == 0);
  }

  /* Restore default state */
  zynq_gpio_output_write(CONFIG_TPL_BOOTSTRAP_TX1_MIO, 1);
  udelay(10);
  zynq_gpio_cfg_input(CONFIG_TPL_BOOTSTRAP_TX1_MIO);
  writel(MIO_CFG_DEFAULT, &slcr_base->mio_pin[CONFIG_TPL_BOOTSTRAP_RX0_MIO]);
  writel(MIO_CFG_DEFAULT, &slcr_base->mio_pin[CONFIG_TPL_BOOTSTRAP_RX1_MIO]);
  writel(MIO_CFG_DEFAULT, &slcr_base->mio_pin[CONFIG_TPL_BOOTSTRAP_TX0_MIO]);
  writel(MIO_CFG_DEFAULT, &slcr_base->mio_pin[CONFIG_TPL_BOOTSTRAP_TX1_MIO]);

  zynq_slcr_lock();

  /* Select an image set to boot */
  const image_set_t *image_set;
  u32 image_table_index;
  if (failsafe) {
    /* Load failsafe image
     * alt = 0 : failsafe A
     * alt = 1 : failsafe B
     */
    u32 offset = alt ? CONFIG_IMAGE_SET_OFFSET_FAILSAFE_B :
                       CONFIG_IMAGE_SET_OFFSET_FAILSAFE_A;
    image_set = (const image_set_t *)(QSPI_LINEAR_START + offset);
    image_table_index = alt ? 0x1 : 0x0;

    if (tpl_image_set_check(image_set) != 0) {
      return -1;
    }

  } else {
    /* Load standard image based on sequence number
     * alt = 0 : most recent
     * alt = 1 : second most recent
     */
    const image_set_t *image_set_a =
        (const image_set_t *)(QSPI_LINEAR_START +
                              CONFIG_IMAGE_SET_OFFSET_STANDARD_A);
    bool image_set_a_valid = (tpl_image_set_check(image_set_a) == 0);

    const image_set_t *image_set_b =
        (const image_set_t *)(QSPI_LINEAR_START +
                              CONFIG_IMAGE_SET_OFFSET_STANDARD_B);
    bool image_set_b_valid = (tpl_image_set_check(image_set_b) == 0);

    if (image_set_a_valid && image_set_b_valid) {
      s32 seq_num_diff = image_set_seq_num_get(image_set_a) -
                         image_set_seq_num_get(image_set_b);
      image_set = ((seq_num_diff >= 0) != alt) ? image_set_a : image_set_b;
    } else if (image_set_a_valid) {
      image_set = image_set_a;
    } else if (image_set_b_valid) {
      image_set = image_set_b;
    } else {
      /* No valid image set */
      return -1;
    }

    image_table_index = (image_set == image_set_a) ? 0x2 : 0x3;
  }

  /* Find SPL image descriptor */
  const image_descriptor_t *image_descriptor;
  err = image_set_descriptor_find(image_set, IMAGE_TYPE_UBOOT_SPL,
                                  &image_descriptor);
  if (err) {
    return err;
  }

  u32 image_data_offset = image_descriptor_data_offset_get(image_descriptor);
  u32 image_data_size = image_descriptor_data_size_get(image_descriptor);
  const u8 *image_data = (const u8 *)(QSPI_LINEAR_START + image_data_offset);

  /* Read header */
  struct image_header header;
  memcpy((void *)&header,
         (const void *)image_data,
         sizeof(struct image_header));

  /* Parse header */
  spl_image.flags |= SPL_COPY_PAYLOAD_ONLY;
  spl_parse_image_header(&header);

  /* Copy image to RAM at load address, skipping over header */
  memcpy((void *)image_descriptor_load_address_get(image_descriptor),
         (const void *)&image_data[sizeof(struct image_header)],
         image_data_size - sizeof(struct image_header));

  /* Write image table index to lower two bits of REBOOT_STATUS */
  u32 reboot_status = readl(REG_REBOOT_STATUS);
  reboot_status = (reboot_status & ~0x3) | (image_table_index & 0x3);
  zynq_slcr_unlock();
  writel(reboot_status, REG_REBOOT_STATUS);
  zynq_slcr_lock();

#else
  /* SPL */
  struct spi_flash *flash;
  flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
                          CONFIG_SF_DEFAULT_CS,
                          CONFIG_SF_DEFAULT_SPEED,
                          CONFIG_SF_DEFAULT_MODE);
  if (!flash) {
    puts("SPI probe failed\n");
    return -ENODEV;
  }

  /* Find U-Boot image descriptor */
  image_descriptor_t image_descriptor;
  err = image_descriptor_get(flash, IMAGE_TYPE_UBOOT, &image_descriptor);
  if (err) {
    puts("Could not find U-Boot image\n");
    return err;
  }

  u32 flash_offset = image_descriptor_data_offset_get(&image_descriptor);

  /* Init image data CRC */
  u32 computed_data_crc;
  image_descriptor_data_crc_init(&computed_data_crc);

  /* Read header */
  struct image_header header;
  err = spi_flash_read(flash, flash_offset,
                       sizeof(struct image_header),
                       (void *)&header);
  if (err) {
    puts("Failed to read U-Boot image header\n");
    return err;
  }

  /* Update image data CRC over header */
  image_descriptor_data_crc_continue(&computed_data_crc, (const u8 *)&header,
                                     sizeof(header));

  /* Parse header */
  spl_image.flags |= SPL_COPY_PAYLOAD_ONLY;
  spl_parse_image_header(&header);

  /* Copy image to RAM at load address, skipping over header */
  u32 load_address = image_descriptor_load_address_get(&image_descriptor);
  u32 load_size = image_descriptor_data_size_get(&image_descriptor) -
                      sizeof(struct image_header);
  err = spi_flash_read(flash, flash_offset + sizeof(struct image_header),
                       load_size, (void *)load_address);
  if (err) {
    puts("Failed to read U-Boot image\n");
    return err;
  }

  /* Update image data CRC over payload */
  image_descriptor_data_crc_continue(&computed_data_crc,
                                     (const u8 *)load_address, load_size);

  /* Check image data CRC */
  if (computed_data_crc != image_descriptor_data_crc_get(&image_descriptor)) {
    puts("U-Boot image CRC failure\n");
    return -1;
  }
#endif

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

int zynq_board_read_rom_ethaddr(unsigned char *ethaddr)
{
#if defined(CONFIG_ZYNQ_GEM_EEPROM_ADDR) && \
    defined(CONFIG_ZYNQ_GEM_I2C_MAC_OFFSET)
	if (eeprom_read(CONFIG_ZYNQ_GEM_EEPROM_ADDR,
			CONFIG_ZYNQ_GEM_I2C_MAC_OFFSET,
			ethaddr, 6))
		printf("I2C EEPROM MAC address read failed\n");
#endif

#if defined(CONFIG_FACTORY_DATA) && \
    defined(CONFIG_ZYNQ_GEM_FACTORY_ADDR) && \
    !defined(CONFIG_TPL_BUILD)
    int i;
    for (i=0; i<6; i++) {
      ethaddr[i] = factory_params.mac_address[6-1-i];
    }
#endif

	return 0;
}

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
