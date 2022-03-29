// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for the Microchip USB2244 Ultra Fast USB 2.0 SD controller.
 *
 * Copyright (C) 2021 Xilinx, Inc.
 */

#include <common.h>
#include <dm.h>
#include <usb.h>
#include <asm/gpio.h>
#include <linux/delay.h>

struct usb2244_priv {
	struct gpio_desc reset_gpio;
};

static int usb2244_probe(struct udevice *dev)
{
	struct usb2244_priv *priv = dev_get_priv(dev);
	int ret;

	ret = dm_gpio_set_value(&priv->reset_gpio, 1);
	if (ret)
		return ret;

	mdelay(5);

	ret = dm_gpio_set_value(&priv->reset_gpio, 0);
	if (ret)
		return ret;

	mdelay(5);

	return 0;
}

static int usb2244_plat(struct udevice *dev)
{
	struct usb2244_priv *priv = dev_get_priv(dev);
	int ret;

	ret = gpio_request_by_name(dev, "reset-gpios", 0, &priv->reset_gpio,
				   GPIOD_IS_OUT | GPIOD_ACTIVE_LOW);
	if (ret) {
		printf("%s, gpio request failed err: %d\n", __func__, ret);
		return ret;
	}

	return 0;
}

U_BOOT_DRIVER(usb2244) = {
	.name = "usb2244",
	.id = UCLASS_MISC,
	.probe = usb2244_probe,
	.of_to_plat = usb2244_plat,
	.priv_auto = sizeof(struct usb2244_priv),
};

static const struct usb_device_id usb2244_id_table[] = {
	{
		.idVendor = 0x0424,
		.idProduct = 0x2240
	},
	{ }
};

U_BOOT_USB_DEVICE(usb2244, usb2244_id_table);
