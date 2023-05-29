#
# Copyright (c) 2023 Michal Simek
#
# SPDX-License-Identifier: GPL-2.0

"""
# Setup env__saveenv_test_skip to True if tests are running in JTAG boot mode.
# For example:
env__saveenv_test_skip = True
"""

import pytest
import random
import ipaddress
import string
import uuid

# Check return code
def ret_code(u_boot_console):
    return u_boot_console.run_command("echo $?")

# Set env variable
def set_env(u_boot_console, var_name, var_value):
    u_boot_console.run_command(f"setenv {var_name} {var_value}")
    assert ret_code(u_boot_console).endswith("0")
    check_env(u_boot_console, var_name, var_value)

# Verify env variable
def check_env(u_boot_console, var_name, var_value):
    if var_value:
        output = u_boot_console.run_command(f"printenv {var_name}")
        var_value = str(var_value)
        if (var_value.startswith("'") and var_value.endswith("'")) or (
            var_value.startswith('"') and var_value.endswith('"')
        ):
            var_value = var_value.split(var_value[-1])[1]
        assert var_value in output
        assert ret_code(u_boot_console).endswith("0")
    else:
        u_boot_console.p.send(f"printenv {var_name}\n")
        output = u_boot_console.p.expect(["not defined"])
        assert output == 0
        assert ret_code(u_boot_console).endswith("1")

@pytest.mark.buildconfigspec("cmd_saveenv")
def test_saveenv(u_boot_console):
    """Test the saveenv command in non-JTAG bootmode.
    It saves the U-Boot environment in persistent storage.
    """

    test_skip = u_boot_console.config.env.get("env__saveenv_test_skip", False)
    if test_skip:
        pytest.skip("saveenv test skipped")

    # Set env for random mac address
    rand_mac = "%02x:%02x:%02x:%02x:%02x:%02x" % (
        random.randint(0, 255),
        random.randint(0, 255),
        random.randint(0, 255),
        random.randint(0, 255),
        random.randint(0, 255),
        random.randint(0, 255),
    )
    set_env(u_boot_console, "mac_addr", rand_mac)

    # Set env for random IPv4 address
    rand_ipv4 = ipaddress.IPv4Address._string_from_ip_int(
        random.randint(0, ipaddress.IPv4Address._ALL_ONES)
    )
    set_env(u_boot_console, "ipv4_addr", rand_ipv4)

    # Set env for random IPv6 address
    rand_ipv6 = ipaddress.IPv6Address._string_from_ip_int(
        random.randint(0, ipaddress.IPv6Address._ALL_ONES)
    )
    set_env(u_boot_console, "ipv6_addr", rand_ipv6)

    # Set env for random number
    rand_num = random.randrange(1, 10**9)
    set_env(u_boot_console, "num_var", rand_num)

    # Set env for uuid
    uuid_str = uuid.uuid4().hex.lower()
    set_env(u_boot_console, "uuid_var", uuid_str)

    # Set env for random string
    schar = "!#%&\()*+,-./:;<=>?@[\\]^_`{|}~"
    rand_str = "".join(
        random.choices(" " + string.ascii_letters + schar + string.digits, k=300)
    )
    set_env(u_boot_console, "str_var", f'"{rand_str}"')

    # Set env for empty string
    set_env(u_boot_console, "empty_var", "")

    # Save the env variables
    u_boot_console.run_command("saveenv")
    assert ret_code(u_boot_console).endswith("0")

    # Reboot
    u_boot_console.run_command("reset", wait_for_reboot=True)

    # Verify the saved env variables
    check_env(u_boot_console, "mac_addr", rand_mac)
    check_env(u_boot_console, "ipv4_addr", rand_ipv4)
    check_env(u_boot_console, "ipv6_addr", rand_ipv6)
    check_env(u_boot_console, "num_var", rand_num)
    check_env(u_boot_console, "uuid_var", uuid_str)
    check_env(u_boot_console, "str_var", rand_str)
    check_env(u_boot_console, "empty_var", "")
