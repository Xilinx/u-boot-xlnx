// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for the Microchip USB5744 4-port hub.
 *
 * Copyright (C) 2021 Xilinx, Inc.
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <i2c.h>
#include <usb.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <asm/global_data.h>

#define I2C_ADDR	0x2D
#define USB_COMMAND_ATTACH	0x0056
#define USB_CONFIG_REG_ACCESS	0x0037

struct usb5744_priv {
	struct gpio_desc reset_gpio;
	struct udevice *i2c_dev;
};

static int usb5744_probe(struct udevice *dev)
{
	struct usb5744_priv *priv = dev_get_priv(dev);
	struct dm_i2c_chip *i2c_chip;
	int ret;
	u32 buf = USB_COMMAND_ATTACH;
	u32 config_reg_access_buf = USB_CONFIG_REG_ACCESS;
	/*
	 *  Prevent the MCU from the putting the HUB in suspend mode through register write.
	 *  The BYPASS_UDC_SUSPEND bit (Bit 3) of the RuntimeFlags2 register at address
	 *  0x411D controls this aspect of the hub.
	 *  Format to write to hub registers via SMBus- 2D 00 00 05 00 01 41 1D 08
	 *  Byte 0: Address of slave 2D
	 *  Byte 1: Memory address 00
	 *  Byte 2: Memory address 00
	 *  Byte 3: Number of bytes to write to memory
	 *  Byte 4: Write configuration register (00)
	 *  Byte 5: Write the number of data bytes (01- 1 data byte)
	 *  Byte 6: LSB of register address 0x41
	 *  Byte 7: MSB of register address 0x1D
	 *  Byte 8: value to be written to the register
	 */
	char data_buf[8] = {0x0, 0x5, 0x0, 0x1, 0x41, 0x1D, 0x08};

	ret = dm_gpio_set_value(&priv->reset_gpio, 1);
	if (ret)
		return ret;

	mdelay(5);

	ret = dm_gpio_set_value(&priv->reset_gpio, 0);
	if (ret)
		return ret;

	mdelay(10);

	i2c_chip = dev_get_parent_plat(priv->i2c_dev);
	if (i2c_chip) {
		i2c_chip->flags &= ~DM_I2C_CHIP_WR_ADDRESS;
		/* SMBus write command */
		ret = dm_i2c_write(priv->i2c_dev, 0x0, (uint8_t *)&data_buf, 8);
		if (ret) {
			dev_err(dev, "data_buf i2c_write failed, err:%d\n", ret);
			return ret;
		}

		/* Configuration register access command */
		ret = dm_i2c_write(priv->i2c_dev, 0x99, (uint8_t *)&config_reg_access_buf, 2);
		if (ret) {
			dev_err(dev, "config_reg_access i2c_write failed, err: %d\n", ret);
			return ret;
		}

		/* USB Attach with SMBus */
		ret = dm_i2c_write(priv->i2c_dev, 0xAA, (uint8_t *)&buf, 2);
		if (ret) {
			dev_err(dev, "usb_attach i2c_write failed, err: %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int usb5744_plat(struct udevice *dev)
{
	struct usb5744_priv *priv = dev_get_priv(dev);
	struct udevice *i2c_bus = NULL;
	ofnode i2c_node;
	u32 phandle;
	int ret;

	ret = gpio_request_by_name(dev, "reset-gpios", 0,
				   &priv->reset_gpio,
				   GPIOD_IS_OUT | GPIOD_ACTIVE_LOW);
	if (ret) {
		dev_err(dev, "Gpio request failed, err: %d\n", ret);
		return ret;
	}

	if (!ofnode_read_u32(dev_ofnode(dev), "i2c-bus", &phandle)) {
		i2c_node = ofnode_get_by_phandle(phandle);
		ret = device_get_global_by_ofnode(i2c_node, &i2c_bus);
		if (ret) {
			dev_err(dev, "Failed to get i2c node, err: %d\n", ret);
			return ret;
		}

		ret = i2c_get_chip(i2c_bus, I2C_ADDR, 1, &priv->i2c_dev);
		if (ret)
			return -ENODEV;
	}

	return 0;
}

U_BOOT_DRIVER(usb5744) = {
	.name = "usb5744",
	.id = UCLASS_MISC,
	.probe = usb5744_probe,
	.of_to_plat = usb5744_plat,
	.priv_auto = sizeof(struct usb5744_priv),
};

static const struct usb_device_id usb5744_id_table[] = {
	{
		.idVendor = 0x0424,
		.idProduct = 0x5744
	},
	{ }
};

U_BOOT_USB_DEVICE(usb5744, usb5744_id_table);
