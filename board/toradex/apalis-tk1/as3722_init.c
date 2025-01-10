// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2012-2016 Toradex, Inc.
 */

#include <log.h>
#include <asm/io.h>
#include <asm/arch-tegra/tegra_i2c.h>
#include <linux/delay.h>

/* AS3722-PMIC-specific early init regs */

#define AS3722_I2C_ADDR		0x80

#define AS3722_SD0VOLTAGE_REG	0x00	/* CPU */
#define AS3722_SD1VOLTAGE_REG	0x01	/* CORE, already set by OTP */
#define AS3722_SD6VOLTAGE_REG	0x06	/* GPU */
#define AS3722_SDCONTROL_REG	0x4D

#define AS3722_LDO1VOLTAGE_REG	0x11	/* VDD_SDMMC1 */
#define AS3722_LDO2VOLTAGE_REG	0x12	/* VPP_FUSE */
#define AS3722_LDO6VOLTAGE_REG	0x16	/* VDD_SDMMC3 */
#define AS3722_LDCONTROL_REG	0x4E

#define AS3722_SD0VOLTAGE_DATA	(0x3C00 | AS3722_SD0VOLTAGE_REG)
#define AS3722_SD0CONTROL_DATA	(0x0100 | AS3722_SDCONTROL_REG)

#define AS3722_SD1VOLTAGE_DATA	(0x3200 | AS3722_SD1VOLTAGE_REG)
#define AS3722_SD1CONTROL_DATA	(0x0200 | AS3722_SDCONTROL_REG)

#define AS3722_SD6CONTROL_DATA	(0x4000 | AS3722_SDCONTROL_REG)
#define AS3722_SD6VOLTAGE_DATA	(0x2800 | AS3722_SD6VOLTAGE_REG)

#define AS3722_LDO1CONTROL_DATA	(0x0200 | AS3722_LDCONTROL_REG)
#define AS3722_LDO1VOLTAGE_DATA	(0x7F00 | AS3722_LDO1VOLTAGE_REG)

#define AS3722_LDO2CONTROL_DATA	(0x0400 | AS3722_LDCONTROL_REG)
#define AS3722_LDO2VOLTAGE_DATA	(0x1000 | AS3722_LDO2VOLTAGE_REG)

#define AS3722_LDO6CONTROL_DATA	(0x4000 | AS3722_LDCONTROL_REG)
#define AS3722_LDO6VOLTAGE_DATA	(0x3F00 | AS3722_LDO6VOLTAGE_REG)

/* AS3722-PMIC-specific early init code - get CPU rails up, etc */

void pmic_enable_cpu_vdd(void)
{
	debug("%s entry\n", __func__);

#ifdef AS3722_SD1VOLTAGE_DATA
	/* Set up VDD_CORE, for boards where OTP is incorrect*/
	debug("%s: Setting VDD_CORE via AS3722 reg 1\n", __func__);
	/* Configure VDD_CORE via the AS3722 PMIC on the PWR I2C bus */
	tegra_i2c_ll_write(AS3722_I2C_ADDR,
			   AS3722_SD1VOLTAGE_DATA);
	/*
	 * Don't write SDCONTROL - it's already 0x7F, i.e. all SDs enabled.
	 * tegra_i2c_ll_write_data(AS3722_SD1CONTROL_DATA, I2C_SEND_2_BYTES);
	 */
	udelay(10 * 1000);
#endif

	/*
	 * Make sure all non-fused regulators are down.
	 * That way we're in known state after software reboot from linux
	 */
	tegra_i2c_ll_write(AS3722_I2C_ADDR, 0x0003);
	udelay(10 * 1000);
	tegra_i2c_ll_write(AS3722_I2C_ADDR, 0x0004);
	udelay(10 * 1000);
	tegra_i2c_ll_write(AS3722_I2C_ADDR, 0x001b);
	udelay(10 * 1000);
	tegra_i2c_ll_write(AS3722_I2C_ADDR, 0x0014);
	udelay(10 * 1000);
	tegra_i2c_ll_write(AS3722_I2C_ADDR, 0x001a);
	udelay(10 * 1000);
	tegra_i2c_ll_write(AS3722_I2C_ADDR, 0x0019);
	udelay(10 * 1000);

	debug("%s: Setting VDD_CPU to 1.0V via AS3722 reg 0/4D\n", __func__);
	/*
	 * Bring up VDD_CPU via the AS3722 PMIC on the PWR I2C bus.
	 * First set VDD to 1.0V, then enable the VDD regulator.
	 */
	tegra_i2c_ll_write(AS3722_I2C_ADDR,
			   AS3722_SD0VOLTAGE_DATA);
	/*
	 * Don't write SDCONTROL - it's already 0x7F, i.e. all SDs enabled.
	 * tegra_i2c_ll_write_data(AS3722_SD0CONTROL_DATA, I2C_SEND_2_BYTES);
	 */
	udelay(10 * 1000);

	debug("%s: Setting VDD_GPU to 1.0V via AS3722 reg 6/4D\n", __func__);
	/*
	 * Bring up VDD_GPU via the AS3722 PMIC on the PWR I2C bus.
	 * First set VDD to 1.0V, then enable the VDD regulator.
	 */
	tegra_i2c_ll_write(AS3722_I2C_ADDR,
			   AS3722_SD6VOLTAGE_DATA);
	/*
	 * Don't write SDCONTROL - it's already 0x7F, i.e. all SDs enabled.
	 * tegra_i2c_ll_write_data(AS3722_SD6CONTROL_DATA, I2C_SEND_2_BYTES);
	 */
	udelay(10 * 1000);

	debug("%s: Set VPP_FUSE to 1.2V via AS3722 reg 0x12/4E\n", __func__);
	/*
	 * Bring up VPP_FUSE via the AS3722 PMIC on the PWR I2C bus.
	 * First set VDD to 1.2V, then enable the VDD regulator.
	 */
	tegra_i2c_ll_write(AS3722_I2C_ADDR,
			   AS3722_LDO2VOLTAGE_DATA);
	/*
	 * Don't write LDCONTROL - it's already 0xFF, i.e. all LDOs enabled.
	 * tegra_i2c_ll_write_data(AS3722_LDO2CONTROL_DATA, I2C_SEND_2_BYTES);
	 */
	udelay(10 * 1000);

	debug("%s: Set VDD_SDMMC1 to 3.3V via AS3722 reg 0x11/4E\n", __func__);
	/*
	 * Bring up VDD_SDMMC1 via the AS3722 PMIC on the PWR I2C bus.
	 * First set it to value closest to 3.3V, then enable the regulator
	 *
	 * NOTE: We do this early because doing it later seems to hose the CPU
	 * power rail/partition startup. Need to debug.
	 */
	tegra_i2c_ll_write(AS3722_I2C_ADDR,
			   AS3722_LDO1VOLTAGE_DATA);
	/*
	 * Don't write LDCONTROL - it's already 0xFF, i.e. all LDOs enabled.
	 * tegra_i2c_ll_write_data(AS3722_LDO1CONTROL_DATA, I2C_SEND_2_BYTES);
	 */
	udelay(10 * 1000);

	debug("%s: Set VDD_SDMMC3 to 3.3V via AS3722 reg 0x16/4E\n", __func__);
	/*
	 * Bring up VDD_SDMMC3 via the AS3722 PMIC on the PWR I2C bus.
	 * First set it to bypass 3.3V straight thru, then enable the regulator
	 *
	 * NOTE: We do this early because doing it later seems to hose the CPU
	 * power rail/partition startup. Need to debug.
	 */
	tegra_i2c_ll_write(AS3722_I2C_ADDR,
			   AS3722_LDO6VOLTAGE_DATA);
	/*
	 * Don't write LDCONTROL - it's already 0xFF, i.e. all LDOs enabled.
	 * tegra_i2c_ll_write_data(AS3722_LDO6CONTROL_DATA, I2C_SEND_2_BYTES);
	 */
	udelay(10 * 1000);
}
