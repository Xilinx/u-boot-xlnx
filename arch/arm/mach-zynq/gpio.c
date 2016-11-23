/*
 * Copyright (C) 2016 Swift Navigation Inc.
 * Contact: Jacob McNamee <jacob@swiftnav.com>
 *
 * This source is subject to the license found in the file 'LICENSE' which must
 * be be distributed together with this source. All other rights reserved.
 *
 * THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
 * EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/hardware.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/clk.h>

void zynq_gpio_cfg_input(unsigned gpio)
{
  u32 reg;

  /* Clear DIRM bit */
  reg = readl(&gpio_base->cfg[gpio / 32].dirm);
  reg &= ~(1 << (gpio % 32));
  writel(reg, &gpio_base->cfg[gpio / 32].dirm);

  /* Clear OEN bit */
  reg = readl(&gpio_base->cfg[gpio / 32].oen);
  reg &= ~(1 << (gpio % 32));
  writel(reg, &gpio_base->cfg[gpio / 32].oen);
}

void zynq_gpio_cfg_output(unsigned gpio)
{
  u32 reg;

  /* Set DIRM bit */
  reg = readl(&gpio_base->cfg[gpio / 32].dirm);
  reg |= (1 << (gpio % 32));
  writel(reg, &gpio_base->cfg[gpio / 32].dirm);

  /* Set OEN bit */
  reg = readl(&gpio_base->cfg[gpio / 32].oen);
  reg |= (1 << (gpio % 32));
  writel(reg, &gpio_base->cfg[gpio / 32].oen);
}

int zynq_gpio_input_read(unsigned gpio)
{
  /* Read DATA_RO bit */
  return (readl(&gpio_base->data_ro[gpio / 32]) >> (gpio % 32)) & 0x1;
}

void zynq_gpio_output_write(unsigned gpio, unsigned value)
{
  u32 reg;

  /* Write DATA bit */
  reg = readl(&gpio_base->data[gpio / 32]);
  reg = (reg & ~(1 << (gpio % 32))) | ((value & 0x1) << (gpio % 32));
  writel(reg, &gpio_base->data[gpio / 32]);
}
