# Copyright (c) 2016, NVIDIA CORPORATION. All rights reserved.
#
# SPDX-License-Identifier: GPL-2.0

# Test various network-related functionality, such as the dhcp, ping, and
# tftpboot commands.

import pytest
import u_boot_utils
import test_net

"""
Note: This test relies on boardenv_* containing configuration values to define
which the network environment available for testing. Without this, this test
will be automatically skipped.

For example:

# Details regarding a file that may be read from a TFTP server. This variable
# may be omitted or set to None if PXE testing is not possible or desired.
env__net_pxe_readable_file = {
    'fn': 'default',
    'addr': 0x10000000,
    'size': 74,
    'timeout': 50000,
    'pattern': 'Linux',
    'valid_label': '1',
    'invalid_label': '2',
    'exp_str_invalid': 'Skipping install for failure retrieving initrd',
    'local_label': '3',
    'exp_str_local': 'missing environment variable: localcmd',
    'empty_label': '4',
    'exp_str_empty': 'No kernel given, skipping boot',
}
"""

@pytest.mark.buildconfigspec('cmd_net')
def test_net_tftpboot_boot(u_boot_console):
    """ Boot loaded image"

    """
    if not test_net.net_set_up:
        pytest.skip('Network not initialized')

    test_net.test_net_dhcp(u_boot_console)
    test_net.test_net_setup_static(u_boot_console)
    test_net.test_net_tftpboot(u_boot_console)

    f = u_boot_console.config.env.get('env__net_tftp_readable_file', None)
    if not f:
        pytest.skip('No TFTP readable file to read')

    addr = f.get('addr', None)
    if not addr:
      addr = u_boot_utils.find_ram_base(u_boot_console)

    timeout = 50000
    with u_boot_console.temporary_timeout(timeout):
        try:
            # wait_for_prompt=False makes the core code not wait for the U-Boot
            # prompt code to be seen, since it won't be on a successful kernel
            # boot
            u_boot_console.run_command('bootm %x' % addr, wait_for_prompt=False)
            # You might want to expand wait_for() with options to add extra bad
            # patterns which immediately indicate a failed boot, or add a new
            # "with object" function u_boot_console.enable_check() that can
            # cause extra patterns like the U-Boot console prompt, U-Boot boot
            # error messages, kernel boot error messages, etc. to fail the
            # wait_for().
            u_boot_console.wait_for('login:')
        finally:
            # This forces the console object to be shutdown, so any subsequent
            # test will reset the board back into U-Boot. We want to force this
            # no matter whether the kernel boot passed or failed.
            u_boot_console.drain_console()
            u_boot_console.cleanup_spawn()

@pytest.mark.buildconfigspec('cmd_net')
def test_net_tftpboot_boot_config2(u_boot_console):
    if not test_net.net_set_up:
        pytest.skip('Network not initialized')

    test_net.test_net_dhcp(u_boot_console)
    test_net.test_net_setup_static(u_boot_console)
    test_net.test_net_tftpboot(u_boot_console)

    f = u_boot_console.config.env.get('env__net_tftp_readable_file', None)
    if not f:
        pytest.skip('No TFTP readable file to read')

    addr = f.get('addr', None)
    if not addr:
      addr = u_boot_utils.find_ram_base(u_boot_console)

    response = u_boot_console.run_command('imi %x' % addr)
    if not "config@2" in response:
        pytest.skip("Second configuration not found");

    timeout = 50000
    with u_boot_console.temporary_timeout(timeout):
        try:
            # wait_for_prompt=False makes the core code not wait for the U-Boot
            # prompt code to be seen, since it won't be on a successful kernel
            # boot
            u_boot_console.run_command('bootm %x#config@2' % addr, wait_for_prompt=False)
            # You might want to expand wait_for() with options to add extra bad
            # patterns which immediately indicate a failed boot, or add a new
            # "with object" function u_boot_console.enable_check() that can
            # cause extra patterns like the U-Boot console prompt, U-Boot boot
            # error messages, kernel boot error messages, etc. to fail the
            # wait_for().
            u_boot_console.wait_for('login:')
        finally:
            # This forces the console object to be shutdown, so any subsequent
            # test will reset the board back into U-Boot. We want to force this
            # no matter whether the kernel boot passed or failed.
            u_boot_console.drain_console()
            u_boot_console.cleanup_spawn()

@pytest.mark.buildconfigspec('cmd_net')
@pytest.mark.buildconfigspec('cmd_pxe')
def test_net_pxe_boot(u_boot_console):
    """Test the pxe command.

    A pxe configuration file is downloaded from the TFTP server and interpreted
    to boot the images mentioned in pxe configuration file.

    The details of the file to download are provided by the boardenv_* file;
    see the comment at the beginning of this file.
    """

    if not test_net.net_set_up:
        pytest.skip('Network not initialized')

    test_net.test_net_dhcp(u_boot_console)
    test_net.test_net_setup_static(u_boot_console)

    f = u_boot_console.config.env.get('env__net_pxe_readable_file', None)
    if not f:
        pytest.skip('No PXE readable file to read')

    addr = f.get('addr', None)
    timeout = f.get('timeout', u_boot_console.p.timeout)
    fn = f['fn']

    with u_boot_console.temporary_timeout(timeout):
        output = u_boot_console.run_command('pxe get')

    expected_text = 'Bytes transferred = '
    sz = f.get('size', None)
    if sz:
        expected_text += '%d' % sz
    assert 'TIMEOUT' not in output
    assert expected_text in output
    assert "Config file 'default.boot' found" in output

    with u_boot_console.temporary_timeout(timeout):
        try:
            # wait_for_prompt=False makes the core code not wait for the U-Boot
            # prompt code to be seen, since it won't be on a successful kernel
            # boot
            if not addr:
                u_boot_console.run_command('pxe boot', wait_for_prompt=False)
            else:
                u_boot_console.run_command('pxe boot %x' % addr, wait_for_prompt=False)
            # You might want to expand wait_for() with options to add extra bad
            # patterns which immediately indicate a failed boot, or add a new
            # "with object" function u_boot_console.enable_check() that can
            # cause extra patterns like the U-Boot console prompt, U-Boot boot
            # error messages, kernel boot error messages, etc. to fail the
            # wait_for().
            u_boot_console.wait_for(f.get('pattern', 'Linux'))
        finally:
            # This forces the console object to be shutdown, so any subsequent
            # test will reset the board back into U-Boot. We want to force this
            # no matter whether the kernel boot passed or failed.
            u_boot_console.drain_console()
            u_boot_console.cleanup_spawn()

@pytest.mark.buildconfigspec('cmd_net')
@pytest.mark.buildconfigspec('cmd_pxe')
def test_net_pxe_config(u_boot_console):
    """Test the pxe command by selecting different combination of labels

    A pxe configuration file is downloaded from the TFTP server and interpreted
    to boot the images mentioned in pxe configuration file.

    The details of the file to download are provided by the boardenv_* file;
    see the comment at the beginning of this file.
    """

    if not test_net.net_set_up:
        pytest.skip('Network not initialized')

    test_net.test_net_dhcp(u_boot_console)
    test_net.test_net_setup_static(u_boot_console)

    f = u_boot_console.config.env.get('env__net_pxe_readable_file', None)
    if not f:
        pytest.skip('No PXE readable file to read')

    addr = f.get('addr', None)
    timeout = f.get('timeout', u_boot_console.p.timeout)
    fn = f['fn']
    local_label = f['local_label']
    empty_label = f['empty_label']
    exp_str_local = f['exp_str_local']
    exp_str_empty = f['exp_str_empty']

    with u_boot_console.temporary_timeout(timeout):
        output = u_boot_console.run_command('pxe get')

    expected_text = 'Bytes transferred = '
    sz = f.get('size', None)
    if sz:
        expected_text += '%d' % sz
    assert 'TIMEOUT' not in output
    assert expected_text in output
    assert "Config file 'default.boot' found" in output

    if not addr:
        pxe_boot_cmd = "pxe boot"
    else:
        pxe_boot_cmd = "pxe boot %x" % addr

    with u_boot_console.temporary_timeout(timeout):
        try:
            # wait_for_prompt=False makes the core code not wait for the U-Boot
            # prompt code to be seen, since it won't be on a successful kernel
            # boot
            u_boot_console.run_command(pxe_boot_cmd, wait_for_prompt=False)

            # pxe config is loaded where multiple labels are there and need to
            # select particular label to boot and check for expected string
            # In this case, local label is selected and it should look for
            # localcmd env variable and if that variable is not defined it
            # should not boot it and come out to u-boot prompt
            u_boot_console.wait_for("Enter choice:")
            u_boot_console.run_command(local_label, wait_for_prompt=False)
            expected_str = u_boot_console.p.expect([exp_str_local])
            assert (
                expected_str == 0
            ), f"Expected string: {exp_str_local} did not match!"

            # In this case, empty label is selected and it should look for
            # kernel image path and if it is not set it should fail it and load
            # default label to boot
            u_boot_console.run_command(pxe_boot_cmd, wait_for_prompt=False)
            u_boot_console.wait_for("Enter choice:")
            u_boot_console.run_command(empty_label, wait_for_prompt=False)
            expected_str = u_boot_console.p.expect([exp_str_empty])
            assert (
                expected_str == 0
            ), f"Expected string: {exp_str_empty} did not match!"

            u_boot_console.wait_for(f.get('pattern', 'Linux'))
        finally:
            # This forces the console object to be shutdown, so any subsequent
            # test will reset the board back into U-Boot. We want to force this
            # no matter whether the kernel boot passed or failed.
            u_boot_console.drain_console()
            u_boot_console.cleanup_spawn()

@pytest.mark.buildconfigspec('cmd_net')
@pytest.mark.buildconfigspec('cmd_pxe')
def test_net_pxe_config2(u_boot_console):
    """Test the pxe command by selecting different combination of labels

    A pxe configuration file is downloaded from the TFTP server and interpreted
    to boot the images mentioned in pxe configuration file.

    The details of the file to download are provided by the boardenv_* file;
    see the comment at the beginning of this file.
    """

    if not test_net.net_set_up:
        pytest.skip('Network not initialized')

    test_net.test_net_dhcp(u_boot_console)
    test_net.test_net_setup_static(u_boot_console)

    f = u_boot_console.config.env.get('env__net_pxe_readable_file', None)
    if not f:
        pytest.skip('No PXE readable file to read')

    addr = f.get('addr', None)
    timeout = f.get('timeout', u_boot_console.p.timeout)
    fn = f['fn']
    invalid_label = f['invalid_label']
    exp_str_invalid = f['exp_str_invalid']

    with u_boot_console.temporary_timeout(timeout):
        output = u_boot_console.run_command('pxe get')

    expected_text = 'Bytes transferred = '
    sz = f.get('size', None)
    if sz:
        expected_text += '%d' % sz
    assert 'TIMEOUT' not in output
    assert expected_text in output
    assert "Config file 'default.boot' found" in output

    if not addr:
        pxe_boot_cmd = "pxe boot"
    else:
        pxe_boot_cmd = "pxe boot %x" % addr

    with u_boot_console.temporary_timeout(timeout):
        try:
            # wait_for_prompt=False makes the core code not wait for the U-Boot
            # prompt code to be seen, since it won't be on a successful kernel
            # boot
            u_boot_console.run_command(pxe_boot_cmd, wait_for_prompt=False)

            # pxe config is loaded where multiple labels are there and need to
            # select particular label to boot and check for expected string
            # In this case invalid label is selected, it should load invalid
            # label and if it fails it should load the default label to boot
            u_boot_console.wait_for("Enter choice:")
            u_boot_console.run_command(invalid_label, wait_for_prompt=False)
            expected_str = u_boot_console.p.expect([exp_str_invalid])
            assert (
                expected_str == 0
            ), f"Expected string: {exp_str_invalid} did not match!"

            u_boot_console.wait_for(f.get('pattern', 'Linux'))
        finally:
            # This forces the console object to be shutdown, so any subsequent
            # test will reset the board back into U-Boot. We want to force this
            # no matter whether the kernel boot passed or failed.
            u_boot_console.drain_console()
            u_boot_console.cleanup_spawn()
