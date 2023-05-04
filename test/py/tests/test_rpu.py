#
# Copyright (c) 2023, Xilinx Inc. Michal Simek
#
# SPDX-License-Identifier: GPL-2.0

import pytest
import random
import string
import test_net

"""
Note: This test relies on boardenv_* containing configuration values to define
RPU applications information which contains, application names, processors,
address where it is built, expected output and tftp load addresses.
It also relies on dhcp configurations to support tftp to load application on
DDR. All the info are stored sequentially. The length of all parameters values
should be same. For example, if 2 app_names are defined the other parameters
also should have 2 values.
It will run RPU cases for all the applications defined in boardenv_*
configuration file.

Example:
env__net_dhcp_server = True

env__rpu_apps = {
    "app_name": "hello_world_r5_0_ddr.elf, hello_world_r5_1_ddr.elf",
    "procs": "rpu0, rpu1",
    "addr": "0xA00000, 0xB00000",
    "output": "Successfully ran Hello World application on DDR from RPU0,
    Successfully ran Hello World application on DDR from RPU1",
    "tftp_addr": "0x100000, 0x200000",
}
"""

# Get rpu apps env var
def get_rpu_apps_env(u_boot_console):
    rpu_apps = u_boot_console.config.env.get("env__rpu_apps", False)
    if not rpu_apps:
        pytest.skip("RPU application info not defined!")

    apps = [x.strip() for x in rpu_apps.get("app_name", None).split(",")]
    if not apps:
        pytes.skip("No RPU application found!")

    procs = [x.strip() for x in rpu_apps.get("procs", None).split(",")]
    if not procs:
        pytes.skip("No RPU application processor provided!")

    addrs = [x.strip() for x in rpu_apps.get("addr", None).split(",")]
    addrs = [int(x, 0) for x in addrs]
    if not addrs:
        pytes.skip("No RPU application build address found!")

    outputs = [x.strip() for x in rpu_apps.get("output", None).split(",")]
    if not outputs:
        pytes.skip("Expected output not found!")

    tftp_addrs = [x.strip() for x in rpu_apps.get("tftp_addr", None).split(",")]
    tftp_addrs = [int(x, 0) for x in tftp_addrs]
    if not tftp_addrs:
        pytes.skip("TFTP address to load application not found!")

    return apps, procs, addrs, outputs, tftp_addrs

# Check return code
def ret_code(u_boot_console):
    return u_boot_console.run_command("echo $?")

# Initialize tcm
def tcminit(u_boot_console, rpu_mode):
    output = u_boot_console.run_command("zynqmp tcminit %s" % rpu_mode)
    assert "Initializing TCM overwrites TCM content" in output
    return ret_code(u_boot_console)

# Load application in DDR
def load_app_ddr(u_boot_console, tftp_addr, app):
    output = u_boot_console.run_command("tftpboot %x %s" % (tftp_addr, app))
    assert "TIMEOUT" not in output
    assert "Bytes transferred = " in output

    # Load elf
    u_boot_console.run_command("bootelf -p %x" % tftp_addr)
    assert ret_code(u_boot_console).endswith("0")

# Get cpu number
def get_cpu_num(proc):
    if proc == "rpu0":
        return 4
    elif proc == "rpu1":
        return 5

# Disable cpus
def disable_cpus(u_boot_console):
    u_boot_console.run_command("cpu 4 disable")
    u_boot_console.run_command("cpu 5 disable")

# Load apps on RPU cores
def rpu_apps_load(u_boot_console, rpu_mode):
    apps, procs, addrs, outputs, tftp_addrs = get_rpu_apps_env(u_boot_console)
    test_net.test_net_dhcp(u_boot_console)
    if not test_net.net_set_up:
        pytest.skip("Network not initialized!")

    try:
        assert tcminit(u_boot_console, rpu_mode).endswith("0")

        for i in range(len(apps)):
            if rpu_mode == "lockstep" and procs[i] != "rpu0":
                continue

            load_app_ddr(u_boot_console, tftp_addrs[i], apps[i])
            rel_addr = int(addrs[i] + 0x3C)

            # Release cpu at app load address
            cpu_num = get_cpu_num(procs[i])
            cmd = "cpu %d release %x %s" % (cpu_num, rel_addr, rpu_mode)
            output = u_boot_console.run_command(cmd)
            assert f"Using TCM jump trampoline for address {hex(rel_addr)}" in output
            assert f"R5 {rpu_mode} mode" in output
            u_boot_console.wait_for(outputs[i])
            assert ret_code(u_boot_console).endswith("0")
    finally:
        disable_cpus(u_boot_console)

def test_rpu_apps_load_split(u_boot_console):
    rpu_apps_load(u_boot_console, "split")

def test_rpu_apps_load_lockstep(u_boot_console):
    rpu_apps_load(u_boot_console, "lockstep")

def test_rpu_apps_load_negative(u_boot_console):
    apps, procs, addrs, outputs, tftp_addrs = get_rpu_apps_env(u_boot_console)

    # Invalid commands
    u_boot_console.run_command("zynqmp tcminit mode")
    assert ret_code(u_boot_console).endswith("1")

    rand_str = "".join(random.choices(string.ascii_lowercase, k=4))
    u_boot_console.run_command("zynqmp tcminit %s" % rand_str)
    assert ret_code(u_boot_console).endswith("1")

    rand_num = random.randint(2, 100)
    u_boot_console.run_command("zynqmp tcminit %d" % rand_num)
    assert ret_code(u_boot_console).endswith("1")

    test_net.test_net_dhcp(u_boot_console)
    if not test_net.net_set_up:
        pytest.skip("Network not initialized!")

    try:
        rpu_mode = "split"
        assert tcminit(u_boot_console, rpu_mode).endswith("0")

        for i in range(len(apps)):
            load_app_ddr(u_boot_console, tftp_addrs[i], apps[i])

            # Run in split mode at different load address
            rel_addr = int(addrs[i]) + random.randint(61, 1000)
            cpu_num = get_cpu_num(procs[i])
            cmd = "cpu %d release %x %s" % (cpu_num, rel_addr, rpu_mode)
            output = u_boot_console.run_command(cmd)
            assert f"Using TCM jump trampoline for address {hex(rel_addr)}" in output
            assert f"R5 {rpu_mode} mode" in output
            assert u_boot_console.p.expect([outputs[i]])

            # Invalid rpu mode
            rand_str = "".join(random.choices(string.ascii_lowercase, k=4))
            cmd = "cpu %d release %x %s" % (cpu_num, rel_addr, rand_str)
            output = u_boot_console.run_command(cmd)
            assert f"Using TCM jump trampoline for address {hex(rel_addr)}" in output
            assert f"Unsupported mode" in output
            assert not ret_code(u_boot_console).endswith("0")

        # Switch to lockstep mode, without disabling CPUs
        rpu_mode = "lockstep"
        u_boot_console.run_command("zynqmp tcminit %s" % rpu_mode)
        assert not ret_code(u_boot_console).endswith("0")

        # Disable cpus
        disable_cpus(u_boot_console)

        # Switch to lockstep mode, after disabling CPUs
        output = u_boot_console.run_command("zynqmp tcminit %s" % rpu_mode)
        assert "Initializing TCM overwrites TCM content" in output
        assert ret_code(u_boot_console).endswith("0")

        # Run lockstep mode for RPU1
        for i in range(len(apps)):
            if procs[i] == "rpu0":
                continue

            load_app_ddr(u_boot_console, tftp_addrs[i], apps[i])
            rel_addr = int(addrs[i] + 0x3C)
            cpu_num = get_cpu_num(procs[i])
            cmd = "cpu %d release %x %s" % (cpu_num, rel_addr, rpu_mode)
            output = u_boot_console.run_command(cmd)
            assert f"Using TCM jump trampoline for address {hex(rel_addr)}" in output
            assert f"R5 {rpu_mode} mode" in output
            assert u_boot_console.p.expect([outputs[i]])
    finally:
        disable_cpus(u_boot_console)
        # This forces the console object to be shutdown, so any subsequent test
        # will reset the board back into U-Boot.
        u_boot_console.drain_console()
        u_boot_console.cleanup_spawn()
