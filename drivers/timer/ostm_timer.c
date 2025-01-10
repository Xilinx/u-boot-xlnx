// SPDX-License-Identifier: GPL-2.0+
/*
 * Renesas RZ/A1 R7S72100 OSTM Timer driver
 *
 * Copyright (C) 2019 Marek Vasut <marek.vasut@gmail.com>
 */

#include <clock_legacy.h>
#include <malloc.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <dm.h>
#include <clk.h>
#include <timer.h>
#include <linux/bitops.h>

#define OSTM_CMP	0x00
#define OSTM_CNT	0x04
#define OSTM_TE		0x10
#define OSTM_TS		0x14
#define OSTM_TT		0x18
#define OSTM_CTL	0x20
#define OSTM_CTL_D	BIT(1)

DECLARE_GLOBAL_DATA_PTR;

struct ostm_priv {
	fdt_addr_t	regs;
};

static u64 ostm_get_count(struct udevice *dev)
{
	struct ostm_priv *priv = dev_get_priv(dev);

	return timer_conv_64(readl(priv->regs + OSTM_CNT));
}

static int ostm_probe(struct udevice *dev)
{
	struct timer_dev_priv *uc_priv = dev_get_uclass_priv(dev);
	struct ostm_priv *priv = dev_get_priv(dev);
#if CONFIG_IS_ENABLED(CLK)
	struct clk clk;
	int ret;

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret)
		return ret;

	uc_priv->clock_rate = clk_get_rate(&clk);
#else
	uc_priv->clock_rate = get_board_sys_clk() / 2;
#endif

	readb(priv->regs + OSTM_CTL);
	writeb(OSTM_CTL_D, priv->regs + OSTM_CTL);

	setbits_8(priv->regs + OSTM_TT, BIT(0));
	writel(0xffffffff, priv->regs + OSTM_CMP);
	setbits_8(priv->regs + OSTM_TS, BIT(0));

	return 0;
}

static int ostm_of_to_plat(struct udevice *dev)
{
	struct ostm_priv *priv = dev_get_priv(dev);

	priv->regs = dev_read_addr(dev);

	return 0;
}

static const struct timer_ops ostm_ops = {
	.get_count	= ostm_get_count,
};

static const struct udevice_id ostm_ids[] = {
	{ .compatible = "renesas,ostm" },
	{}
};

U_BOOT_DRIVER(ostm_timer) = {
	.name		= "ostm-timer",
	.id		= UCLASS_TIMER,
	.ops		= &ostm_ops,
	.probe		= ostm_probe,
	.of_match	= ostm_ids,
	.of_to_plat = ostm_of_to_plat,
	.priv_auto	= sizeof(struct ostm_priv),
};
