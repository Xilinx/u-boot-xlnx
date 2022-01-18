// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for the Microchip USB5744 4-port hub.
 *
 * Copyright (C) 2021 Xilinx, Inc.
 */

#include <common.h>
#include <dm.h>
#include <i2c.h>
#include <usb.h>
#include <asm/gpio.h>
#include <linux/delay.h>
#include <asm/global_data.h>

#define I2C_ADDR	0x2D
#define I2C_DATA	0x0056

struct usb5744_priv {
	struct gpio_desc reset_gpio;
	struct udevice *i2c_dev;
};

static int usb5744_probe(struct udevice *dev)
{
	struct usb5744_priv *priv = dev_get_priv(dev);
	struct dm_i2c_chip *i2c_chip;
	int ret;
	u32 buf = I2C_DATA;

	dm_gpio_set_value(&priv->reset_gpio, 1);
	mdelay(5);
	dm_gpio_set_value(&priv->reset_gpio, 0);
	mdelay(10);

	i2c_chip = dev_get_parent_plat(priv->i2c_dev);
	if (i2c_chip) {
		i2c_chip->flags &= ~DM_I2C_CHIP_WR_ADDRESS;
		ret = dm_i2c_write(priv->i2c_dev, 0xAA, (uint8_t *)&buf, 2);
		if (ret) {
			printf("i2c_write failed, err: %d\n", ret);
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
				   &priv->reset_gpio, GPIOD_ACTIVE_LOW);
	if (ret) {
		printf("Gpio request failed, err: %d\n", ret);
		return ret;
	}

	if (!ofnode_read_u32(dev_ofnode(dev), "i2c-bus", &phandle)) {
		i2c_node = ofnode_get_by_phandle(phandle);
		ret = device_get_global_by_ofnode(i2c_node, &i2c_bus);
		if (ret) {
			printf("Failed to get i2c node, err: %d\n", ret);
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

