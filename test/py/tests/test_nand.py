# SPDX-License-Identifier: GPL-2.0
# (C) Copyright 2023, Advanced Micro Devices, Inc.

import pytest
import random
import re
import u_boot_utils

"""
Note: This test relies on boardenv_* containing configuration values to define
the nand device total size and timeout available for testing. Without this, the
test will be automatically skipped. This test will be also skipped if the NAND
flash device is not detected.

For example:

# Setup env__nand_device_test to set the NAND flash total size and timeout.
env__nand_device_test = {
   'size': '8192 MB',
   'timeout': 100000,
}
"""

# Find out nand memory parameters
def nand_pre_commands(u_boot_console):
    f = u_boot_console.config.env.get('env__nand_device_test', None)
    if not f:
        pytest.skip('No env file to read for NAND device test')

    total_size = f.get('size', None)
    timeout = f.get('timeout')

    if not total_size:
        pytest.skip('NAND device size not recognized')

    output = u_boot_console.run_command('nand info')
    if not 'Device 0: nand0' in output:
        pytest.skip('No NAND device available')

    m = re.search('Page size (.+?) b', output)
    if m:
        try:
            page_size = int(m.group(1))
        except ValueError:
            pytest.fail('NAND page size not recognized')

    m = re.search('sector size (.+?) KiB', output)
    if m:
        try:
            erase_size = int(m.group(1))
            sector_size = erase_size
        except ValueError:
            pytest.fail('NAND erase size not recognized')

        erase_size *= 1024

    output = u_boot_console.run_command('nand bad')
    if not 'bad blocks:' in output:
        pytest.skip('No NAND device available')

    count = 0
    m = re.search('bad blocks:([\n\s\s\d\w]*)', output)
    if m:
        print(m.group(1))
        var = m.group(1).split()
        count = len(var)
    print('bad blocks count= ' + str(count))

    m = re.search('(.+?) MB', total_size)
    if m:
        try:
            total_size = int(m.group(1))
            total_size *= 1024 * 1024
            print('Total size is: ' + str(total_size) + ' B')
            total_size -= count * sector_size * 1024
            print('New Total size is: ' + str(total_size) + ' B')
        except ValueError:
            pytest.fail('NAND size not recognized')

    return page_size, erase_size, total_size, sector_size, timeout

@pytest.mark.buildconfigspec('cmd_nand')
@pytest.mark.buildconfigspec('cmd_bdi')
@pytest.mark.buildconfigspec('cmd_memory')
def test_nand_read_twice(u_boot_console):
    """This test reads the whole NAND flash twice, random_size till full flash
    size, random till page size.
    """

    page_size, erase_size, total_size, sector_size, timeout = nand_pre_commands(
            u_boot_console)
    expected_read = 'read: OK'

    for size in random.randint(4, page_size), random.randint(4, total_size), total_size:
        addr = u_boot_utils.find_ram_base(u_boot_console)

        output = u_boot_console.run_command(
            'nand read %x 0 %x' % (addr + total_size, size)
        )
        assert expected_read in output

        output = u_boot_console.run_command('crc32 %x %x' % (addr + total_size, size))
        m = re.search('==> (.+?)', output)
        if not m:
            pytest.fail('CRC32 failed')
        expected_crc32 = m.group(1)

        output = u_boot_console.run_command(
            'nand read %x 0 %x' % (addr + total_size + 10, size)
        )
        assert expected_read in output

        output = u_boot_console.run_command(
            'crc32 %x %x' % (addr + total_size + 10, size)
        )
        assert expected_crc32 in output

@pytest.mark.buildconfigspec('cmd_nand')
@pytest.mark.buildconfigspec('cmd_bdi')
@pytest.mark.buildconfigspec('cmd_memory')
def test_nand_write_twice(u_boot_console):
    """This test does the random writes till page size, size and full size"""

    page_size, erase_size, total_size, sector_size, timeout = nand_pre_commands(
            u_boot_console)
    expected_write = 'written: OK'
    expected_read = 'read: OK'
    expected_erase = '100% complete.'
    old_size = 0

    for size in (
        random.randint(4, page_size),
        random.randint(page_size, total_size),
        total_size,
    ):
        offset = page_size
        addr = u_boot_utils.find_ram_base(u_boot_console)
        size = size - old_size
        output = u_boot_console.run_command('crc32 %x %x' % (addr + total_size, size))
        m = re.search('==> (.+?)', output)
        if not m:
            pytest.fail('CRC32 failed')

        expected_crc32 = m.group(1)

        if old_size % page_size:
            old_size = int(old_size / page_size + 1)
            old_size *= page_size

        if old_size + size > total_size:
            size = total_size - old_size

        eraseoffset = int(old_size / erase_size)
        eraseoffset *= erase_size

        erasesize = int(size / erase_size + 1)
        erasesize *= erase_size

        output = u_boot_console.run_command(
            'nand erase.spread %x %x' % (eraseoffset, erasesize)
        )
        assert expected_erase in output

        output = u_boot_console.run_command(
            'nand write %x %x %x' % (addr + total_size, old_size, size)
        )
        assert expected_write in output
        output = u_boot_console.run_command(
            'nand read %x %x %x' % (addr + total_size + offset, old_size, size)
        )
        assert expected_read in output
        output = u_boot_console.run_command(
            'crc32 %x %x' % (addr + total_size + offset, size)
        )
        assert expected_crc32 in output
        old_size = size

@pytest.mark.buildconfigspec('cmd_nand')
def test_nand_erase_block(u_boot_console):
    page_size, erase_size, total_size, sector_size, timeout = nand_pre_commands(
            u_boot_console)

    expected_erase = '100% complete.'
    for start in range(0, total_size, erase_size):
        output = u_boot_console.run_command(
            'nand erase.spread %x %x' % (start, erase_size)
        )
        assert expected_erase in output

@pytest.mark.buildconfigspec('cmd_nand')
def test_nand_erase_all(u_boot_console):
    page_size, erase_size, total_size, sector_size, timeout = nand_pre_commands(
            u_boot_console)

    expected_erase = '100% complete.'
    start = 0
    with u_boot_console.temporary_timeout(timeout):
        output = u_boot_console.run_command(
            'nand erase.spread 0 ' + str(hex(total_size))
        )
        assert expected_erase in output
