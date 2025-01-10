// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2015  Thomas Chou <thomas@wytron.com.tw>
 * Copyright (C) 2011  Missing Link Electronics
 *                     Joachim Foerster <joachim@missinglinkelectronics.com>
 */
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <fdtdec.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/gpio.h>

DECLARE_GLOBAL_DATA_PTR;

struct altera_pio_regs {
	u32	data;			/* Data register */
	u32	direction;		/* Direction register */
};

struct altera_pio_plat {
	struct altera_pio_regs *regs;
	int gpio_count;
	const char *bank_name;
};

static int altera_pio_direction_input(struct udevice *dev, unsigned pin)
{
	struct altera_pio_plat *plat = dev_get_plat(dev);
	struct altera_pio_regs *const regs = plat->regs;

	clrbits_le32(&regs->direction, 1 << pin);

	return 0;
}

static int altera_pio_direction_output(struct udevice *dev, unsigned pin,
				     int val)
{
	struct altera_pio_plat *plat = dev_get_plat(dev);
	struct altera_pio_regs *const regs = plat->regs;

	if (val)
		setbits_le32(&regs->data, 1 << pin);
	else
		clrbits_le32(&regs->data, 1 << pin);
	/* change the data first, then the direction. to avoid glitch */
	setbits_le32(&regs->direction, 1 << pin);

	return 0;
}

static int altera_pio_get_value(struct udevice *dev, unsigned pin)
{
	struct altera_pio_plat *plat = dev_get_plat(dev);
	struct altera_pio_regs *const regs = plat->regs;

	return !!(readl(&regs->data) & (1 << pin));
}

static int altera_pio_set_value(struct udevice *dev, unsigned pin, int val)
{
	struct altera_pio_plat *plat = dev_get_plat(dev);
	struct altera_pio_regs *const regs = plat->regs;

	if (val)
		setbits_le32(&regs->data, 1 << pin);
	else
		clrbits_le32(&regs->data, 1 << pin);

	return 0;
}

static int altera_pio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct altera_pio_plat *plat = dev_get_plat(dev);

	uc_priv->gpio_count = plat->gpio_count;
	uc_priv->bank_name = plat->bank_name;

	return 0;
}

static int altera_pio_of_to_plat(struct udevice *dev)
{
	struct altera_pio_plat *plat = dev_get_plat(dev);

	plat->regs = map_physmem(dev_read_addr(dev),
				 sizeof(struct altera_pio_regs),
				 MAP_NOCACHE);
	plat->gpio_count = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
		"altr,gpio-bank-width", 32);
	plat->bank_name = fdt_getprop(gd->fdt_blob, dev_of_offset(dev),
		"gpio-bank-name", NULL);

	return 0;
}

static const struct dm_gpio_ops altera_pio_ops = {
	.direction_input	= altera_pio_direction_input,
	.direction_output	= altera_pio_direction_output,
	.get_value		= altera_pio_get_value,
	.set_value		= altera_pio_set_value,
};

static const struct udevice_id altera_pio_ids[] = {
	{ .compatible = "altr,pio-1.0" },
	{ }
};

U_BOOT_DRIVER(altera_pio) = {
	.name		= "altera_pio",
	.id		= UCLASS_GPIO,
	.of_match	= altera_pio_ids,
	.ops		= &altera_pio_ops,
	.of_to_plat = altera_pio_of_to_plat,
	.plat_auto	= sizeof(struct altera_pio_plat),
	.probe		= altera_pio_probe,
};
