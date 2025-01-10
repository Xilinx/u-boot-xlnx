// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Amarula Solutions.
 * Author: Jagan Teki <jagan@amarulasolutions.com>
 */

#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <clk/sunxi.h>
#include <dt-bindings/clock/sun50i-a64-ccu.h>
#include <dt-bindings/reset/sun50i-a64-ccu.h>
#include <linux/bitops.h>

static const struct ccu_clk_gate a64_gates[] = {
	[CLK_PLL_PERIPH0]	= GATE(0x028, BIT(31)),

	[CLK_BUS_MIPI_DSI]	= GATE(0x060, BIT(1)),
	[CLK_BUS_MMC0]		= GATE(0x060, BIT(8)),
	[CLK_BUS_MMC1]		= GATE(0x060, BIT(9)),
	[CLK_BUS_MMC2]		= GATE(0x060, BIT(10)),
	[CLK_BUS_NAND]		= GATE(0x060, BIT(13)),
	[CLK_BUS_EMAC]		= GATE(0x060, BIT(17)),
	[CLK_BUS_SPI0]		= GATE(0x060, BIT(20)),
	[CLK_BUS_SPI1]		= GATE(0x060, BIT(21)),
	[CLK_BUS_OTG]		= GATE(0x060, BIT(23)),
	[CLK_BUS_EHCI0]		= GATE(0x060, BIT(24)),
	[CLK_BUS_EHCI1]		= GATE(0x060, BIT(25)),
	[CLK_BUS_OHCI0]		= GATE(0x060, BIT(28)),
	[CLK_BUS_OHCI1]		= GATE(0x060, BIT(29)),

	[CLK_BUS_TCON0]		= GATE(0x064, BIT(3)),
	[CLK_BUS_TCON1]		= GATE(0x064, BIT(4)),
	[CLK_BUS_HDMI]		= GATE(0x064, BIT(11)),
	[CLK_BUS_DE]		= GATE(0x064, BIT(12)),

	[CLK_BUS_PIO]		= GATE(0x068, BIT(5)),

	[CLK_BUS_I2C0]		= GATE(0x06c, BIT(0)),
	[CLK_BUS_I2C1]		= GATE(0x06c, BIT(1)),
	[CLK_BUS_I2C2]		= GATE(0x06c, BIT(2)),
	[CLK_BUS_UART0]		= GATE(0x06c, BIT(16)),
	[CLK_BUS_UART1]		= GATE(0x06c, BIT(17)),
	[CLK_BUS_UART2]		= GATE(0x06c, BIT(18)),
	[CLK_BUS_UART3]		= GATE(0x06c, BIT(19)),
	[CLK_BUS_UART4]		= GATE(0x06c, BIT(20)),

	[CLK_NAND]		= GATE(0x080, BIT(31)),
	[CLK_SPI0]		= GATE(0x0a0, BIT(31)),
	[CLK_SPI1]		= GATE(0x0a4, BIT(31)),

	[CLK_USB_PHY0]		= GATE(0x0cc, BIT(8)),
	[CLK_USB_PHY1]		= GATE(0x0cc, BIT(9)),
	[CLK_USB_HSIC]		= GATE(0x0cc, BIT(10)),
	[CLK_USB_HSIC_12M]	= GATE(0x0cc, BIT(11)),
	[CLK_USB_OHCI0]		= GATE(0x0cc, BIT(16)),
	[CLK_USB_OHCI1]		= GATE(0x0cc, BIT(17)),

	[CLK_DE]		= GATE(0x104, BIT(31)),
	[CLK_TCON0]		= GATE(0x118, BIT(31)),
	[CLK_TCON1]		= GATE(0x11c, BIT(31)),

	[CLK_HDMI]		= GATE(0x150, BIT(31)),
	[CLK_HDMI_DDC]		= GATE(0x154, BIT(31)),

	[CLK_DSI_DPHY]		= GATE(0x168, BIT(15)),
};

static const struct ccu_reset a64_resets[] = {
	[RST_USB_PHY0]          = RESET(0x0cc, BIT(0)),
	[RST_USB_PHY1]          = RESET(0x0cc, BIT(1)),
	[RST_USB_HSIC]          = RESET(0x0cc, BIT(2)),

	[RST_BUS_MIPI_DSI]	= RESET(0x2c0, BIT(1)),
	[RST_BUS_MMC0]		= RESET(0x2c0, BIT(8)),
	[RST_BUS_MMC1]		= RESET(0x2c0, BIT(9)),
	[RST_BUS_MMC2]		= RESET(0x2c0, BIT(10)),
	[RST_BUS_NAND]		= RESET(0x2c0, BIT(13)),
	[RST_BUS_EMAC]		= RESET(0x2c0, BIT(17)),
	[RST_BUS_SPI0]		= RESET(0x2c0, BIT(20)),
	[RST_BUS_SPI1]		= RESET(0x2c0, BIT(21)),
	[RST_BUS_OTG]           = RESET(0x2c0, BIT(23)),
	[RST_BUS_EHCI0]         = RESET(0x2c0, BIT(24)),
	[RST_BUS_EHCI1]         = RESET(0x2c0, BIT(25)),
	[RST_BUS_OHCI0]         = RESET(0x2c0, BIT(28)),
	[RST_BUS_OHCI1]         = RESET(0x2c0, BIT(29)),

	[RST_BUS_TCON0]		= RESET(0x2c4, BIT(3)),
	[RST_BUS_TCON1]		= RESET(0x2c4, BIT(4)),
	[RST_BUS_HDMI0]		= RESET(0x2c4, BIT(10)),
	[RST_BUS_HDMI1]		= RESET(0x2c4, BIT(11)),
	[RST_BUS_DE]		= RESET(0x2c4, BIT(12)),

	[RST_BUS_I2C0]		= RESET(0x2d8, BIT(0)),
	[RST_BUS_I2C1]		= RESET(0x2d8, BIT(1)),
	[RST_BUS_I2C2]		= RESET(0x2d8, BIT(2)),
	[RST_BUS_UART0]		= RESET(0x2d8, BIT(16)),
	[RST_BUS_UART1]		= RESET(0x2d8, BIT(17)),
	[RST_BUS_UART2]		= RESET(0x2d8, BIT(18)),
	[RST_BUS_UART3]		= RESET(0x2d8, BIT(19)),
	[RST_BUS_UART4]		= RESET(0x2d8, BIT(20)),
};

const struct ccu_desc a64_ccu_desc = {
	.gates = a64_gates,
	.resets = a64_resets,
	.num_gates = ARRAY_SIZE(a64_gates),
	.num_resets = ARRAY_SIZE(a64_resets),
};
