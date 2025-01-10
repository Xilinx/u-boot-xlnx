// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2022 Theobroma Systems Design und Consulting GmbH
 */

#include <asm/gpio.h>
#include <linux/delay.h>
#include "../common/common.h"

int rockchip_early_misc_init_r(void)
{
	setup_boottargets();

	return 0;
}

#define STM32_RST	100 /* GPIO3_A4 */
#define STM32_BOOT	101 /* GPIO3_A5 */

void spl_board_init(void)
{
	/*
	 * Glitches on STM32_BOOT and STM32_RST lines during poweroff or power
	 * on may put the STM32 companion microcontroller into DFU mode, let's
	 * always reset it into normal mode instead.
	 * Toggling the STM32_RST line is safe to do with the ATtiny companion
	 * microcontroller variant because it will not trigger an MCU reset
	 * since only a UPDI reset command will. Since a UPDI reset is difficult
	 * to mistakenly trigger, glitches to the lines are theoretically also
	 * incapable of triggering an actual ATtiny reset.
	 */
	int ret;

	ret = gpio_request(STM32_RST, "STM32_RST");
	if (ret) {
		debug("Failed to request STM32_RST\n");
		return;
	}

	ret = gpio_request(STM32_BOOT, "STM32_BOOT");
	if (ret) {
		debug("Failed to request STM32_BOOT\n");
		return;
	}

	/* Rely on HW pull-down for inactive level */
	ret = gpio_direction_input(STM32_BOOT);
	if (ret) {
		debug("Failed to configure STM32_BOOT as input\n");
		return;
	}

	ret = gpio_direction_output(STM32_RST, 0);
	if (ret) {
		debug("Failed to configure STM32_RST as output low\n");
		return;
	}

	mdelay(1);

	ret = gpio_direction_input(STM32_RST);
	if (ret) {
		debug("Failed to configure STM32_RST as input\n");
		return;
	}
}
