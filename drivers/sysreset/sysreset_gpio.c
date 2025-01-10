// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 Xilinx, Inc. - Michal Simek
 */

#include <dm.h>
#include <errno.h>
#include <log.h>
#include <sysreset.h>
#include <asm/gpio.h>

struct gpio_reboot_priv {
	struct gpio_desc gpio;
};

static int gpio_reboot_request(struct udevice *dev, enum sysreset_t type)
{
	struct gpio_reboot_priv *priv = dev_get_priv(dev);
	int ret;

	/*
	 * When debug log is enabled please make sure that chars won't end up
	 * in output fifo. Or you can append udelay(); to get enough time
	 * to HW to emit output fifo.
	 */
	debug("GPIO reset\n");

	/* Writing 1 respects polarity (active high/low) based on gpio->flags */
	ret = dm_gpio_set_value(&priv->gpio, 1);
	if (ret < 0)
		return ret;

	return -EINPROGRESS;
}

static struct sysreset_ops gpio_reboot_ops = {
	.request = gpio_reboot_request,
};

static int gpio_reboot_probe(struct udevice *dev)
{
	struct gpio_reboot_priv *priv = dev_get_priv(dev);

	/*
	 * Linux kernel DT binding contain others optional properties
	 * which are not supported now
	 */

	return gpio_request_by_name(dev, "gpios", 0, &priv->gpio, GPIOD_IS_OUT);
}

static const struct udevice_id led_gpio_ids[] = {
	{ .compatible = "gpio-restart" },
	{ }
};

U_BOOT_DRIVER(gpio_reboot) = {
	.id = UCLASS_SYSRESET,
	.name = "gpio_restart",
	.of_match = led_gpio_ids,
	.ops = &gpio_reboot_ops,
	.priv_auto	= sizeof(struct gpio_reboot_priv),
	.probe = gpio_reboot_probe,
};
