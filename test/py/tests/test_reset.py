#
# Copyright (c) 2022 Michal Simek
#
# SPDX-License-Identifier: GPL-2.0

"""
# Setup env__reset_test_skip to True if tests with i2c devices should be
# skipped. For example: JTAG boot mode
env__reset_test_skip = True
"""

import pytest
import random
import u_boot_utils

def test_reset(u_boot_console):
    """Test the reset command in non-JTAG bootmode.
    It does COLD reset, which resets CPU, DDR and peripherals
    """

    test_skip = u_boot_console.config.env.get('env__reset_test_skip', False)
    if test_skip:
        pytest.skip('reset cold test skipped')

    u_boot_console.run_command("reset", wait_for_prompt=False)
    autoboot_prompt = "Hit any key to stop autoboot"
    output = u_boot_console.p.expect([autoboot_prompt])
    u_boot_console.p.send(" ")
    assert output == 0, f"Expected string: {autoboot_prompt} did not match!"

def test_reset_w(u_boot_console):
    """Test the reset -w command in non-JTAG bootmode.
    It does WARM reset, which resets CPU but keep DDR/peripherals active.
    """

    test_skip = u_boot_console.config.env.get('env__reset_test_skip', False)
    if test_skip:
        pytest.skip('reset warm test skipped')

    u_boot_console.run_command("reset -w", wait_for_prompt=False)
    autoboot_prompt = "Hit any key to stop autoboot"
    output = u_boot_console.p.expect([autoboot_prompt])
    u_boot_console.p.send(" ")
    assert output == 0, f"Expected string: {autoboot_prompt} did not match!"
