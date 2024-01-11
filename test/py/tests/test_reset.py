# SPDX-License-Identifier: GPL-2.0
# (C) Copyright 2023, Advanced Micro Devices, Inc.

"""
Note: This test doesn't rely on boardenv_* configuration value but they can
change test behavior.

# Setup env__reset_test_skip to True if reset test is not possible or desired
# and should be skipped.
env__reset_test_skip = True

# This test will be also skipped if the bootmode is detected to JTAG.
"""

import pytest
import re
import test_000_version

def setup_reset_env(u_boot_console):
    if u_boot_console.config.env.get('env__reset_test_skip', False):
        pytest.skip('reset test is not enabled')

    output = u_boot_console.run_command('print modeboot')
    m = re.search('modeboot=(.+?)boot', output)
    if not m:
        pytest.skip('bootmode cannnot be determined')

    bootmode = m.group(1)
    if bootmode == 'jtag':
        pytest.skip('skipping reset test due to jtag bootmode')

def test_reset(u_boot_console):
    """Test the reset command in non-JTAG bootmode.
    It does COLD reset, which resets CPU, DDR and peripherals
    """
    setup_reset_env(u_boot_console)
    u_boot_console.run_command('reset', wait_for_reboot=True)

    # Checks the u-boot command prompt's functionality after reset
    test_000_version.test_version(u_boot_console)

def test_reset_w(u_boot_console):
    """Test the reset -w command in non-JTAG bootmode.
    It does WARM reset, which resets CPU but keep DDR/peripherals active.
    """
    setup_reset_env(u_boot_console)
    u_boot_console.run_command('reset -w', wait_for_reboot=True)

    # Checks the u-boot command prompt's functionality after reset
    test_000_version.test_version(u_boot_console)
