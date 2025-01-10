// SPDX-License-Identifier: GPL-2.0
/* Renesas Ethernet SERDES device driver
 *
 * Copyright (C) 2022 Renesas Electronics Corporation
 */

#include <asm/io.h>
#include <clk-uclass.h>
#include <clk.h>
#include <div64.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <dm/of_access.h>
#include <generic-phy.h>
#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/iopoll.h>
#include <log.h>
#include <reset.h>
#include <syscon.h>

#define R8A779F0_ETH_SERDES_NUM			3
#define R8A779F0_ETH_SERDES_OFFSET		0x0400
#define R8A779F0_ETH_SERDES_BANK_SELECT		0x03fc
#define R8A779F0_ETH_SERDES_TIMEOUT_US		100000
#define R8A779F0_ETH_SERDES_NUM_RETRY_LINKUP	3

struct r8a779f0_eth_serdes_drv_data;
struct r8a779f0_eth_serdes_channel {
	struct r8a779f0_eth_serdes_drv_data *dd;
	struct phy *phy;
	void __iomem *addr;
	phy_interface_t phy_interface;
	int speed;
	int index;
};

struct r8a779f0_eth_serdes_drv_data {
	void __iomem *addr;
	struct reset_ctl *reset;
	struct r8a779f0_eth_serdes_channel channel[R8A779F0_ETH_SERDES_NUM];
	bool initialized;
};

/*
 * The datasheet describes initialization procedure without any information
 * about registers' name/bits. So, this is all black magic to initialize
 * the hardware.
 */
static void r8a779f0_eth_serdes_write32(void __iomem *addr, u32 offs, u32 bank, u32 data)
{
	writel(bank, addr + R8A779F0_ETH_SERDES_BANK_SELECT);
	writel(data, addr + offs);
}

static int
r8a779f0_eth_serdes_reg_wait(struct r8a779f0_eth_serdes_channel *channel,
			     u32 offs, u32 bank, u32 mask, u32 expected)
{
	u32 val = 0;
	int ret;

	writel(bank, channel->addr + R8A779F0_ETH_SERDES_BANK_SELECT);

	ret = readl_poll_timeout(channel->addr + offs, val,
				 (val & mask) == expected,
				 R8A779F0_ETH_SERDES_TIMEOUT_US);
	if (ret)
		dev_dbg(channel->phy->dev,
			"%s: index %d, offs %x, bank %x, mask %x, expected %x\n",
			 __func__, channel->index, offs, bank, mask, expected);

	return ret;
}

static int
r8a779f0_eth_serdes_common_init_ram(struct r8a779f0_eth_serdes_drv_data *dd)
{
	struct r8a779f0_eth_serdes_channel *channel;
	int i, ret;

	for (i = 0; i < R8A779F0_ETH_SERDES_NUM; i++) {
		channel = &dd->channel[i];
		ret = r8a779f0_eth_serdes_reg_wait(channel, 0x026c, 0x180, BIT(0), 0x01);
		if (ret)
			return ret;
	}

	r8a779f0_eth_serdes_write32(dd->addr, 0x026c, 0x180, 0x03);

	return ret;
}

static int
r8a779f0_eth_serdes_common_setting(struct r8a779f0_eth_serdes_channel *channel)
{
	struct r8a779f0_eth_serdes_drv_data *dd = channel->dd;

	switch (channel->phy_interface) {
	case PHY_INTERFACE_MODE_SGMII:
		r8a779f0_eth_serdes_write32(dd->addr, 0x0244, 0x180, 0x0097);
		r8a779f0_eth_serdes_write32(dd->addr, 0x01d0, 0x180, 0x0060);
		r8a779f0_eth_serdes_write32(dd->addr, 0x01d8, 0x180, 0x2200);
		r8a779f0_eth_serdes_write32(dd->addr, 0x01d4, 0x180, 0x0000);
		r8a779f0_eth_serdes_write32(dd->addr, 0x01e0, 0x180, 0x003d);
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

static int
r8a779f0_eth_serdes_chan_setting(struct r8a779f0_eth_serdes_channel *channel)
{
	int ret;

	switch (channel->phy_interface) {
	case PHY_INTERFACE_MODE_SGMII:
		r8a779f0_eth_serdes_write32(channel->addr, 0x0000, 0x380, 0x2000);
		r8a779f0_eth_serdes_write32(channel->addr, 0x01c0, 0x180, 0x0011);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0248, 0x180, 0x0540);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0258, 0x180, 0x0015);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0144, 0x180, 0x0100);
		r8a779f0_eth_serdes_write32(channel->addr, 0x01a0, 0x180, 0x0000);
		r8a779f0_eth_serdes_write32(channel->addr, 0x00d0, 0x180, 0x0002);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0150, 0x180, 0x0003);
		r8a779f0_eth_serdes_write32(channel->addr, 0x00c8, 0x180, 0x0100);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0148, 0x180, 0x0100);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0174, 0x180, 0x0000);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0160, 0x180, 0x0007);
		r8a779f0_eth_serdes_write32(channel->addr, 0x01ac, 0x180, 0x0000);
		r8a779f0_eth_serdes_write32(channel->addr, 0x00c4, 0x180, 0x0310);
		r8a779f0_eth_serdes_write32(channel->addr, 0x00c8, 0x180, 0x0101);
		ret = r8a779f0_eth_serdes_reg_wait(channel, 0x00c8, 0x0180, BIT(0), 0);
		if (ret)
			return ret;

		r8a779f0_eth_serdes_write32(channel->addr, 0x0148, 0x180, 0x0101);
		ret = r8a779f0_eth_serdes_reg_wait(channel, 0x0148, 0x0180, BIT(0), 0);
		if (ret)
			return ret;

		r8a779f0_eth_serdes_write32(channel->addr, 0x00c4, 0x180, 0x1310);
		r8a779f0_eth_serdes_write32(channel->addr, 0x00d8, 0x180, 0x1800);
		r8a779f0_eth_serdes_write32(channel->addr, 0x00dc, 0x180, 0x0000);
		r8a779f0_eth_serdes_write32(channel->addr, 0x001c, 0x300, 0x0001);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0000, 0x380, 0x2100);
		ret = r8a779f0_eth_serdes_reg_wait(channel, 0x0000, 0x0380, BIT(8), 0);
		if (ret)
			return ret;

		if (channel->speed == 1000)
			r8a779f0_eth_serdes_write32(channel->addr, 0x0000, 0x1f00, 0x0140);
		else if (channel->speed == 100)
			r8a779f0_eth_serdes_write32(channel->addr, 0x0000, 0x1f00, 0x2100);

		/* For AN_ON */
		r8a779f0_eth_serdes_write32(channel->addr, 0x0004, 0x1f80, 0x0005);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0028, 0x1f80, 0x07a1);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0000, 0x1f80, 0x0208);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int
r8a779f0_eth_serdes_chan_speed(struct r8a779f0_eth_serdes_channel *channel)
{
	int ret;

	switch (channel->phy_interface) {
	case PHY_INTERFACE_MODE_SGMII:
		/* For AN_ON */
		if (channel->speed == 1000)
			r8a779f0_eth_serdes_write32(channel->addr, 0x0000, 0x1f00, 0x1140);
		else if (channel->speed == 100)
			r8a779f0_eth_serdes_write32(channel->addr, 0x0000, 0x1f00, 0x3100);
		ret = r8a779f0_eth_serdes_reg_wait(channel, 0x0008, 0x1f80, BIT(0), 1);
		if (ret)
			return ret;
		r8a779f0_eth_serdes_write32(channel->addr, 0x0008, 0x1f80, 0x0000);
		break;
	default:
		return -EOPNOTSUPP;
	}

	return 0;
}

static int r8a779f0_eth_serdes_monitor_linkup(struct r8a779f0_eth_serdes_channel *channel)
{
	int i, ret;

	for (i = 0; i < R8A779F0_ETH_SERDES_NUM_RETRY_LINKUP; i++) {
		ret = r8a779f0_eth_serdes_reg_wait(channel, 0x0004, 0x300,
						   BIT(2), BIT(2));
		if (!ret)
			break;

		/* restart */
		r8a779f0_eth_serdes_write32(channel->addr, 0x0144, 0x180, 0x0100);
		udelay(1);
		r8a779f0_eth_serdes_write32(channel->addr, 0x0144, 0x180, 0x0000);
	}

	return ret;
}

static int r8a779f0_eth_serdes_hw_init(struct r8a779f0_eth_serdes_channel *channel)
{
	struct r8a779f0_eth_serdes_drv_data *dd = channel->dd;
	int i, ret;

	if (dd->initialized)
		return 0;

	ret = r8a779f0_eth_serdes_common_init_ram(dd);
	if (ret)
		return ret;

	for (i = 0; i < R8A779F0_ETH_SERDES_NUM; i++) {
		ret = r8a779f0_eth_serdes_reg_wait(&dd->channel[i], 0x0000,
						   0x300, BIT(15), 0);
		if (ret)
			return ret;
	}

	for (i = 0; i < R8A779F0_ETH_SERDES_NUM; i++)
		r8a779f0_eth_serdes_write32(dd->channel[i].addr, 0x03d4, 0x380, 0x0443);

	ret = r8a779f0_eth_serdes_common_setting(channel);
	if (ret)
		return ret;

	for (i = 0; i < R8A779F0_ETH_SERDES_NUM; i++)
		r8a779f0_eth_serdes_write32(dd->channel[i].addr, 0x03d0, 0x380, 0x0001);

	r8a779f0_eth_serdes_write32(dd->addr, 0x0000, 0x380, 0x8000);

	ret = r8a779f0_eth_serdes_common_init_ram(dd);
	if (ret)
		return ret;

	return r8a779f0_eth_serdes_reg_wait(&dd->channel[0], 0x0000, 0x380, BIT(15), 0);
}

static int r8a779f0_eth_serdes_init(struct phy *p)
{
	struct r8a779f0_eth_serdes_drv_data *dd = dev_get_priv(p->dev);
	struct r8a779f0_eth_serdes_channel *channel = dd->channel + p->id;
	int ret;

	ret = r8a779f0_eth_serdes_hw_init(channel);
	if (!ret)
		channel->dd->initialized = true;

	return ret;
}

static int r8a779f0_eth_serdes_hw_init_late(struct r8a779f0_eth_serdes_channel *channel)
{
	int ret;

	ret = r8a779f0_eth_serdes_chan_setting(channel);
	if (ret)
		return ret;

	ret = r8a779f0_eth_serdes_chan_speed(channel);
	if (ret)
		return ret;

	r8a779f0_eth_serdes_write32(channel->addr, 0x03c0, 0x380, 0x0000);

	r8a779f0_eth_serdes_write32(channel->addr, 0x03d0, 0x380, 0x0000);

	return r8a779f0_eth_serdes_monitor_linkup(channel);
}

static int r8a779f0_eth_serdes_power_on(struct phy *p)
{
	struct r8a779f0_eth_serdes_drv_data *dd = dev_get_priv(p->dev);
	struct r8a779f0_eth_serdes_channel *channel = dd->channel + p->id;

	return r8a779f0_eth_serdes_hw_init_late(channel);
}

static int r8a779f0_eth_serdes_set_mode(struct phy *p, enum phy_mode mode,
					int submode)
{
	struct r8a779f0_eth_serdes_drv_data *dd = dev_get_priv(p->dev);
	struct r8a779f0_eth_serdes_channel *channel = dd->channel + p->id;

	if (mode != PHY_MODE_ETHERNET)
		return -EOPNOTSUPP;

	switch (submode) {
	case PHY_INTERFACE_MODE_GMII:
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_USXGMII:
		channel->phy_interface = submode;
		return 0;
	default:
		return -EOPNOTSUPP;
	}
}

static int r8a779f0_eth_serdes_set_speed(struct phy *p, int speed)
{
	struct r8a779f0_eth_serdes_drv_data *dd = dev_get_priv(p->dev);
	struct r8a779f0_eth_serdes_channel *channel = dd->channel + p->id;

	channel->speed = speed;

	return 0;
}

static int r8a779f0_eth_serdes_of_xlate(struct phy *phy,
					struct ofnode_phandle_args *args)
{
	if (args->args_count < 1)
		return -ENODEV;

	if (args->args[0] >= R8A779F0_ETH_SERDES_NUM)
		return -ENODEV;

	phy->id = args->args[0];

	return 0;
}

static const struct phy_ops r8a779f0_eth_serdes_ops = {
	.init		= r8a779f0_eth_serdes_init,
	.power_on	= r8a779f0_eth_serdes_power_on,
	.set_mode	= r8a779f0_eth_serdes_set_mode,
	.set_speed	= r8a779f0_eth_serdes_set_speed,
	.of_xlate	= r8a779f0_eth_serdes_of_xlate,
};

static const struct udevice_id r8a779f0_eth_serdes_of_table[] = {
	{ .compatible = "renesas,r8a779f0-ether-serdes", },
	{ }
};

static int r8a779f0_eth_serdes_probe(struct udevice *dev)
{
	struct r8a779f0_eth_serdes_drv_data *dd = dev_get_priv(dev);
	int i;

	dd->addr = dev_read_addr_ptr(dev);
	if (!dd->addr)
		return -EINVAL;

	dd->reset = devm_reset_control_get(dev, NULL);
	if (IS_ERR(dd->reset))
		return PTR_ERR(dd->reset);

	reset_assert(dd->reset);
	reset_deassert(dd->reset);

	for (i = 0; i < R8A779F0_ETH_SERDES_NUM; i++) {
		struct r8a779f0_eth_serdes_channel *channel = &dd->channel[i];

		channel->addr = dd->addr + R8A779F0_ETH_SERDES_OFFSET * i;
		channel->dd = dd;
		channel->index = i;
	}

	return 0;
}

U_BOOT_DRIVER(r8a779f0_eth_serdes_driver_platform) = {
	.name		= "r8a779f0_eth_serdes",
	.id		= UCLASS_PHY,
	.of_match	= r8a779f0_eth_serdes_of_table,
	.probe		= r8a779f0_eth_serdes_probe,
	.ops		= &r8a779f0_eth_serdes_ops,
	.priv_auto	= sizeof(struct r8a779f0_eth_serdes_drv_data),
};
