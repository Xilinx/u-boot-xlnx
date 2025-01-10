// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Rockchip USB2.0 PHY with Innosilicon IP block driver
 *
 * Copyright (C) 2016 Fuzhou Rockchip Electronics Co., Ltd
 * Copyright (C) 2020 Amarula Solutions(India)
 */

#include <clk-uclass.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <generic-phy.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/arch-rockchip/clock.h>

#define usleep_range(a, b) udelay((b))
#define BIT_WRITEABLE_SHIFT	16

enum rockchip_usb2phy_port_id {
	USB2PHY_PORT_OTG,
	USB2PHY_PORT_HOST,
	USB2PHY_NUM_PORTS,
};

struct usb2phy_reg {
	unsigned int	offset;
	unsigned int	bitend;
	unsigned int	bitstart;
	unsigned int	disable;
	unsigned int	enable;
};

struct rockchip_usb2phy_port_cfg {
	struct usb2phy_reg	phy_sus;
};

struct rockchip_usb2phy_cfg {
	unsigned int reg;
	struct usb2phy_reg	clkout_ctl;
	const struct rockchip_usb2phy_port_cfg port_cfgs[USB2PHY_NUM_PORTS];
};

struct rockchip_usb2phy {
	struct regmap *reg_base;
	struct clk phyclk;
	const struct rockchip_usb2phy_cfg *phy_cfg;
};

static inline int property_enable(struct regmap *base,
				  const struct usb2phy_reg *reg, bool en)
{
	unsigned int val, mask, tmp;

	if (!reg->offset && !reg->enable && !reg->disable)
		return 0;

	tmp = en ? reg->enable : reg->disable;
	mask = GENMASK(reg->bitend, reg->bitstart);
	val = (tmp << reg->bitstart) | (mask << BIT_WRITEABLE_SHIFT);

	return regmap_write(base, reg->offset, val);
}

static inline bool property_enabled(struct regmap *base,
				    const struct usb2phy_reg *reg)
{
	int ret;
	unsigned int tmp, orig;
	unsigned int mask = GENMASK(reg->bitend, reg->bitstart);

	if (!reg->offset && !reg->enable && !reg->disable)
		return false;

	ret = regmap_read(base, reg->offset, &orig);
	if (ret)
		return false;

	tmp = (orig & mask) >> reg->bitstart;
	return tmp != reg->disable;
}

static const
struct rockchip_usb2phy_port_cfg *us2phy_get_port(struct phy *phy)
{
	struct udevice *parent = dev_get_parent(phy->dev);
	struct rockchip_usb2phy *priv = dev_get_priv(parent);
	const struct rockchip_usb2phy_cfg *phy_cfg = priv->phy_cfg;

	return &phy_cfg->port_cfgs[phy->id];
}

static int rockchip_usb2phy_power_on(struct phy *phy)
{
	struct udevice *parent = dev_get_parent(phy->dev);
	struct rockchip_usb2phy *priv = dev_get_priv(parent);
	const struct rockchip_usb2phy_port_cfg *port_cfg = us2phy_get_port(phy);

	property_enable(priv->reg_base, &port_cfg->phy_sus, false);

	/* waiting for the utmi_clk to become stable */
	usleep_range(1500, 2000);

	return 0;
}

static int rockchip_usb2phy_power_off(struct phy *phy)
{
	struct udevice *parent = dev_get_parent(phy->dev);
	struct rockchip_usb2phy *priv = dev_get_priv(parent);
	const struct rockchip_usb2phy_port_cfg *port_cfg = us2phy_get_port(phy);

	property_enable(priv->reg_base, &port_cfg->phy_sus, true);

	return 0;
}

static int rockchip_usb2phy_init(struct phy *phy)
{
	struct udevice *parent = dev_get_parent(phy->dev);
	struct rockchip_usb2phy *priv = dev_get_priv(parent);
	int ret;

	ret = clk_enable(&priv->phyclk);
	if (ret && ret != -ENOSYS) {
		dev_err(phy->dev, "failed to enable phyclk (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}

static int rockchip_usb2phy_exit(struct phy *phy)
{
	struct udevice *parent = dev_get_parent(phy->dev);
	struct rockchip_usb2phy *priv = dev_get_priv(parent);

	clk_disable(&priv->phyclk);

	return 0;
}

static int rockchip_usb2phy_of_xlate(struct phy *phy,
				     struct ofnode_phandle_args *args)
{
	const char *name = phy->dev->name;

	if (!strcasecmp(name, "host-port"))
		phy->id = USB2PHY_PORT_HOST;
	else if (!strcasecmp(name, "otg-port"))
		phy->id = USB2PHY_PORT_OTG;
	else
		dev_err(phy->dev, "improper %s device\n", name);

	return 0;
}

static struct phy_ops rockchip_usb2phy_ops = {
	.init = rockchip_usb2phy_init,
	.exit = rockchip_usb2phy_exit,
	.power_on = rockchip_usb2phy_power_on,
	.power_off = rockchip_usb2phy_power_off,
	.of_xlate = rockchip_usb2phy_of_xlate,
};

/**
 * round_rate() - Adjust a rate to the exact rate a clock can provide.
 * @clk:	The clock to manipulate.
 * @rate:	Desidered clock rate in Hz.
 *
 * Return: rounded rate in Hz, or -ve error code.
 */
ulong rockchip_usb2phy_clk_round_rate(struct clk *clk, ulong rate)
{
	return 480000000;
}

/**
 * enable() - Enable a clock.
 * @clk:	The clock to manipulate.
 *
 * Return: zero on success, or -ve error code.
 */
int rockchip_usb2phy_clk_enable(struct clk *clk)
{
	struct udevice *parent = dev_get_parent(clk->dev);
	struct rockchip_usb2phy *priv = dev_get_priv(parent);
	const struct rockchip_usb2phy_cfg *phy_cfg = priv->phy_cfg;

	/* turn on 480m clk output if it is off */
	if (!property_enabled(priv->reg_base, &phy_cfg->clkout_ctl)) {
		property_enable(priv->reg_base, &phy_cfg->clkout_ctl, true);

		/* waiting for the clk become stable */
		usleep_range(1200, 1300);
	}

	return 0;
}

/**
 * disable() - Disable a clock.
 * @clk:	The clock to manipulate.
 *
 * Return: zero on success, or -ve error code.
 */
int rockchip_usb2phy_clk_disable(struct clk *clk)
{
	struct udevice *parent = dev_get_parent(clk->dev);
	struct rockchip_usb2phy *priv = dev_get_priv(parent);
	const struct rockchip_usb2phy_cfg *phy_cfg = priv->phy_cfg;

	/* turn off 480m clk output */
	property_enable(priv->reg_base, &phy_cfg->clkout_ctl, false);

	return 0;
}

static struct clk_ops rockchip_usb2phy_clk_ops = {
	.enable = rockchip_usb2phy_clk_enable,
	.disable = rockchip_usb2phy_clk_disable,
	.round_rate = rockchip_usb2phy_clk_round_rate
};

static int rockchip_usb2phy_probe(struct udevice *dev)
{
	struct rockchip_usb2phy *priv = dev_get_priv(dev);
	const struct rockchip_usb2phy_cfg *phy_cfgs;
	unsigned int reg;
	int index, ret;

	if (dev_read_bool(dev, "rockchip,usbgrf"))
		priv->reg_base =
			syscon_regmap_lookup_by_phandle(dev, "rockchip,usbgrf");
	else
		priv->reg_base = syscon_get_regmap(dev_get_parent(dev));
	if (IS_ERR(priv->reg_base))
		return PTR_ERR(priv->reg_base);

	ret = ofnode_read_u32_index(dev_ofnode(dev), "reg", 0, &reg);
	if (ret) {
		dev_err(dev, "failed to read reg property (ret = %d)\n", ret);
		return ret;
	}

	/* support address_cells=2 */
	if (dev_read_addr_cells(dev) == 2 && reg == 0) {
		if (ofnode_read_u32_index(dev_ofnode(dev), "reg", 1, &reg)) {
			dev_err(dev, "%s must have reg[1]\n",
				ofnode_get_name(dev_ofnode(dev)));
			return -EINVAL;
		}
	}

	phy_cfgs = (const struct rockchip_usb2phy_cfg *)
					dev_get_driver_data(dev);
	if (!phy_cfgs)
		return -EINVAL;

	/* find out a proper config which can be matched with dt. */
	index = 0;
	do {
		if (phy_cfgs[index].reg == reg) {
			priv->phy_cfg = &phy_cfgs[index];
			break;
		}

		++index;
	} while (phy_cfgs[index].reg);

	if (!priv->phy_cfg) {
		dev_err(dev, "failed find proper phy-cfg\n");
		return -EINVAL;
	}

	ret = clk_get_by_name(dev, "phyclk", &priv->phyclk);
	if (ret) {
		dev_err(dev, "failed to get the phyclk (ret=%d)\n", ret);
		return ret;
	}

	return 0;
}

static int rockchip_usb2phy_bind(struct udevice *dev)
{
	struct udevice *usb2phy_dev;
	ofnode node;
	const char *name;
	int ret = 0;

	dev_for_each_subnode(node, dev) {
		if (!ofnode_is_enabled(node))
			continue;

		name = ofnode_get_name(node);
		dev_dbg(dev, "subnode %s\n", name);

		ret = device_bind_driver_to_node(dev, "rockchip_usb2phy_port",
						 name, node, &usb2phy_dev);
		if (ret) {
			dev_err(dev,
				"'%s' cannot bind 'rockchip_usb2phy_port'\n", name);
			goto bind_fail;
		}
	}

	node = dev_ofnode(dev);
	name = "clk_usbphy_480m";
	dev_read_string_index(dev, "clock-output-names", 0, &name);

	dev_dbg(dev, "clk %s for node %s\n", name, ofnode_get_name(node));

	ret = device_bind_driver_to_node(dev, "rockchip_usb2phy_clock",
					 name, node, &usb2phy_dev);
	if (ret) {
		dev_err(dev,
			"'%s' cannot bind 'rockchip_usb2phy_clock'\n", name);
		goto bind_fail;
	}

	return 0;

bind_fail:
	device_chld_unbind(dev, NULL);

	return ret;
}

static const struct rockchip_usb2phy_cfg rk3308_phy_cfgs[] = {
	{
		.reg = 0x100,
		.clkout_ctl	= { 0x0108, 4, 4, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus	= { 0x0100, 1, 0, 2, 1 },
			},
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0x0104, 1, 0, 2, 1 },
			}
		},
	},
	{ /* sentinel */ }
};

static const struct rockchip_usb2phy_cfg rk3328_usb2phy_cfgs[] = {
	{
		.reg = 0x100,
		.clkout_ctl	= { 0x0108, 4, 4, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus	= { 0x0100, 1, 0, 2, 1 },
			},
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0x0104, 1, 0, 2, 1 },
			}
		},
	},
	{ /* sentinel */ }
};

static const struct rockchip_usb2phy_cfg rk3399_usb2phy_cfgs[] = {
	{
		.reg		= 0xe450,
		.clkout_ctl	= { 0xe450, 4, 4, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus	= { 0xe454, 1, 0, 2, 1 },
			},
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0xe458, 1, 0, 2, 1 },
			}
		},
	},
	{
		.reg		= 0xe460,
		.clkout_ctl	= { 0xe460, 4, 4, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus        = { 0xe464, 1, 0, 2, 1 },
			},
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0xe468, 1, 0, 2, 1 },
			}
		},
	},
	{ /* sentinel */ }
};

static const struct rockchip_usb2phy_cfg rk3568_phy_cfgs[] = {
	{
		.reg		= 0xfe8a0000,
		.clkout_ctl	= { 0x0008, 4, 4, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus	= { 0x0000, 1, 0, 2, 1 },
			},
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0x0004, 1, 0, 2, 1 },
			}
		},
	},
	{
		.reg		= 0xfe8b0000,
		.clkout_ctl	= { 0x0008, 4, 4, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus	= { 0x0000, 1, 0, 2, 1 },
			},
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0x0004, 1, 0, 2, 1 },
			}
		},
	},
	{ /* sentinel */ }
};

static const struct rockchip_usb2phy_cfg rk3588_phy_cfgs[] = {
	{
		.reg		= 0x0000,
		.clkout_ctl	= { 0x0000, 0, 0, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus	= { 0x000c, 11, 11, 0, 1 },
			}
		},
	},
	{
		.reg		= 0x4000,
		.clkout_ctl	= { 0x0000, 0, 0, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus	= { 0x000c, 11, 11, 0, 1 },
			}
		},
	},
	{
		.reg		= 0x8000,
		.clkout_ctl	= { 0x0000, 0, 0, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0x0008, 2, 2, 0, 1 },
			}
		},
	},
	{
		.reg		= 0xc000,
		.clkout_ctl	= { 0x0000, 0, 0, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0x0008, 2, 2, 0, 1 },
			}
		},
	},
	{ /* sentinel */ }
};

static const struct udevice_id rockchip_usb2phy_ids[] = {
	{
		.compatible = "rockchip,rk3308-usb2phy",
		.data = (ulong)&rk3308_phy_cfgs,
	},
	{
		.compatible = "rockchip,rk3328-usb2phy",
		.data = (ulong)&rk3328_usb2phy_cfgs,
	},
	{
		.compatible = "rockchip,rk3399-usb2phy",
		.data = (ulong)&rk3399_usb2phy_cfgs,
	},
	{
		.compatible = "rockchip,rk3568-usb2phy",
		.data = (ulong)&rk3568_phy_cfgs,
	},
	{
		.compatible = "rockchip,rk3588-usb2phy",
		.data = (ulong)&rk3588_phy_cfgs,
	},
	{ /* sentinel */ }
};

U_BOOT_DRIVER(rockchip_usb2phy_port) = {
	.name		= "rockchip_usb2phy_port",
	.id		= UCLASS_PHY,
	.ops		= &rockchip_usb2phy_ops,
};

U_BOOT_DRIVER(rockchip_usb2phy_clock) = {
	.name		= "rockchip_usb2phy_clock",
	.id		= UCLASS_CLK,
	.ops		= &rockchip_usb2phy_clk_ops,
};

U_BOOT_DRIVER(rockchip_usb2phy) = {
	.name	= "rockchip_usb2phy",
	.id	= UCLASS_PHY,
	.of_match = rockchip_usb2phy_ids,
	.probe = rockchip_usb2phy_probe,
	.bind = rockchip_usb2phy_bind,
	.priv_auto	= sizeof(struct rockchip_usb2phy),
};
