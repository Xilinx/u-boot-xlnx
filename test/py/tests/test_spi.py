# SPDX-License-Identifier: GPL-2.0
# (C) Copyright 2023, Advanced Micro Devices, Inc.

import pytest
import random
import re
import u_boot_utils

"""
Note: This test relies on boardenv_* containing configuration values to define
spi minimum and maximum frequnecies at which the flash part can operate on and
these tests run at 5 different spi frequnecy randomised values in the range.
It also defines the SPI bus number containing the SPI-flash chip, SPI
chip-select, SPI mode, SPI flash part name and timeout parameters. If minimum
and maximum frequency is not defined, it will run on freq 0 by default.

Without the boardenv_* configuration, this test will be automatically skipped.

It also relies on configuration values for supported flashes for qspi lock and
unlock cases. It will run qspi lock-unlock cases only for the supported flash
parts.

Example:
env__spi_device_test = {
    'bus': 0,
    'chip_select': 0,
    'min_freq': 10000000,
    'max_freq': 100000000,
    'mode': 0,
    'part_name': 'n25q00a',
    'timeout': 100000,
}

env__qspi_lock_unlock = {
    'supported_flash': 'mt25qu512a, n25q00a, n25q512ax3',
}
"""

def setup_spi(u_boot_console):
    f = u_boot_console.config.env.get('env__spi_device_test', None)
    if not f:
        pytest.skip('No env file to read for SPI family device test')

    bus = f.get('bus', 0)
    cs = f.get('chip_select', 0)
    mode = f.get('mode', 0)
    part_name = f.get('part_name', None)
    timeout = f.get('timeout', None)

    if not part_name:
        pytest.skip('No env file to read SPI family flash part name')

    return bus, cs, mode, part_name, timeout

# Find out minimum and maximum frequnecies that SPI device can operate
def spi_find_freq_range(u_boot_console):
    f = u_boot_console.config.env.get('env__spi_device_test', None)
    if not f:
        pytest.skip('No env file to read for SPI family device test')

    min_f = f.get('min_freq', None)
    max_f = f.get('max_freq', None)

    if not min_f:
        min_f = 0
    if not max_f:
        max_f = 0
    if max_f < min_f:
        max_f = min_f

    if min_f == 0 and max_f == 0:
        iterations = 1
    else:
        iterations = 5

    return min_f, max_f, iterations

# Find out SPI family flash memory parameters
def spi_pre_commands(u_boot_console, freq):
    bus, cs, mode, part_name, timeout = setup_spi(u_boot_console)

    output = u_boot_console.run_command(f'sf probe {bus}:{cs} {freq} {mode}')
    if not 'SF: Detected' in output:
        pytest.skip('No SPI device available')

    if not part_name in output:
        pytest.fail('SPI flash part name not recognized')

    m = re.search('page size (.+?) Bytes', output)
    if m:
        try:
            page_size = int(m.group(1))
        except ValueError:
            pytest.fail('SPI page size not recognized')

    m = re.search('erase size (.+?) KiB', output)
    if m:
        try:
            erase_size = int(m.group(1))
        except ValueError:
            pytest.fail('SPI erase size not recognized')

        erase_size *= 1024

    m = re.search('total (.+?) MiB', output)
    if m:
        try:
            global total_size
            total_size = int(m.group(1))
        except ValueError:
            pytest.fail('SPI total size not recognized')

        total_size *= 1024 * 1024

    m = re.search('Detected (.+?) with', output)
    if m:
        try:
            flash_part = m.group(1)
            assert flash_part == part_name
        except:
            pytest.fail('SPI flash part not recognized')

    return page_size, erase_size, total_size, flash_part, timeout

# Read the whole SPI flash twice, random_size till full flash size, random
# till page size
def spi_read_twice(u_boot_console, page_size, total_size, timeout):
    expected_read = 'Read: OK'

    for size in random.randint(4, page_size), random.randint(4, total_size), total_size:
        addr = u_boot_utils.find_ram_base(u_boot_console)
        size = size & ~3
        with u_boot_console.temporary_timeout(timeout):
            output = u_boot_console.run_command(
                'sf read %x 0 %x' % (addr + total_size, size)
            )
            assert expected_read in output
        output = u_boot_console.run_command('crc32 %x %x' % (addr + total_size, size))
        m = re.search('==> (.+?)$', output)
        if not m:
            pytest.fail('CRC32 failed')
        expected_crc32 = m.group(1)
        with u_boot_console.temporary_timeout(timeout):
            output = u_boot_console.run_command(
                'sf read %x 0 %x' % (addr + total_size + 10, size)
            )
            assert expected_read in output
        output = u_boot_console.run_command(
            'crc32 %x %x' % (addr + total_size + 10, size)
        )
        assert expected_crc32 in output

@pytest.mark.buildconfigspec('cmd_sf')
@pytest.mark.buildconfigspec('cmd_bdi')
@pytest.mark.buildconfigspec('cmd_memory')
def test_spi_read_twice(u_boot_console):
    min_f, max_f, loop = spi_find_freq_range(u_boot_console)
    i = 0
    while i < loop:
        page_size, erase_size, total_size, flash_part, timeout = spi_pre_commands(
                u_boot_console, random.randint(min_f, max_f))
        spi_read_twice(u_boot_console, page_size, total_size, timeout)
        i = i + 1

# This test check crossing boundary for dual/parallel configurations
def spi_erase_block(u_boot_console, erase_size, total_size):
    expected_erase = 'Erased: OK'
    for start in range(0, total_size, erase_size):
        output = u_boot_console.run_command('sf erase %x %x' % (start, erase_size))
        assert expected_erase in output

@pytest.mark.buildconfigspec('cmd_sf')
def test_spi_erase_block(u_boot_console):
    min_f, max_f, loop = spi_find_freq_range(u_boot_console)
    i = 0
    while i < loop:
        page_size, erase_size, total_size, flash_part, timeout = spi_pre_commands(
                u_boot_console, random.randint(min_f, max_f))
        spi_erase_block(u_boot_console, erase_size, total_size)
        i = i + 1

# Random write till page size, random till size and full size
def spi_write_twice(u_boot_console, page_size, erase_size, total_size, timeout):
    addr = u_boot_utils.find_ram_base(u_boot_console)
    expected_write = 'Written: OK'
    expected_read = 'Read: OK'
    expected_erase = 'Erased: OK'

    old_size = 0
    for size in (
        random.randint(4, page_size),
        random.randint(page_size, total_size),
        total_size,
    ):
        offset = random.randint(4, page_size)
        offset = offset & ~3
        size = size & ~3
        size = size - old_size
        output = u_boot_console.run_command('crc32 %x %x' % (addr + total_size, size))
        m = re.search('==> (.+?)$', output)
        if not m:
            pytest.fail('CRC32 failed')

        expected_crc32 = m.group(1)
        if old_size % page_size:
            old_size = int(old_size / page_size)
            old_size *= page_size

        if size % erase_size:
            erasesize = int(size / erase_size + 1)
            erasesize *= erase_size

        eraseoffset = int(old_size / erase_size)
        eraseoffset *= erase_size

        timeout = 100000000
        start = 0
        with u_boot_console.temporary_timeout(timeout):
            output = u_boot_console.run_command(
                'sf erase %x %x' % (eraseoffset, erasesize)
            )
            assert expected_erase in output

        with u_boot_console.temporary_timeout(timeout):
            output = u_boot_console.run_command(
                'sf write %x %x %x' % (addr + total_size, old_size, size)
            )
            assert expected_write in output
        with u_boot_console.temporary_timeout(timeout):
            output = u_boot_console.run_command(
                'sf read %x %x %x' % (addr + total_size + offset, old_size, size)
            )
            assert expected_read in output
        output = u_boot_console.run_command(
            'crc32 %x %x' % (addr + total_size + offset, size)
        )
        assert expected_crc32 in output
        old_size = size

@pytest.mark.buildconfigspec('cmd_bdi')
@pytest.mark.buildconfigspec('cmd_sf')
@pytest.mark.buildconfigspec('cmd_memory')
def test_spi_write_twice(u_boot_console):
    min_f, max_f, loop = spi_find_freq_range(u_boot_console)
    i = 0
    while i < loop:
        page_size, erase_size, total_size, flash_part, timeout = spi_pre_commands(
                u_boot_console, random.randint(min_f, max_f))
        spi_write_twice(u_boot_console, page_size, erase_size, total_size, timeout)
        i = i + 1

def spi_write_continues(u_boot_console, page_size, total_size, timeout):
    spi_erase_block(u_boot_console)
    expected_write = 'Written: OK'
    expected_read = 'Read: OK'
    addr = u_boot_utils.find_ram_base(u_boot_console)

    output = u_boot_console.run_command('crc32 %x %x' % (addr + 0x10000, total_size))
    m = re.search('==> (.+?)$', output)
    if not m:
        pytest.fail('CRC32 failed')
    expected_crc32 = m.group(1)

    old_size = 0
    for size in (
        random.randint(4, page_size),
        random.randint(page_size, total_size),
        total_size,
    ):
        size = size & ~3
        size = size - old_size
        with u_boot_console.temporary_timeout(timeout):
            output = u_boot_console.run_command(
                'sf write %x %x %x' % (addr + 0x10000 + old_size, old_size, size)
            )
            assert expected_write in output
        old_size += size

    with u_boot_console.temporary_timeout(timeout):
        output = u_boot_console.run_command(
            'sf read %x %x %x' % (addr + 0x10000 + total_size, 0, total_size)
        )
        assert expected_read in output

    output = u_boot_console.run_command(
        'crc32 %x %x' % (addr + 0x10000 + total_size, total_size)
    )
    assert expected_crc32 in output

@pytest.mark.buildconfigspec('cmd_bdi')
@pytest.mark.buildconfigspec('cmd_sf')
@pytest.mark.buildconfigspec('cmd_memory')
def test_spi_write_continues(u_boot_console):
    min_f, max_f, loop = spi_find_freq_range(u_boot_console)
    i = 0
    while i < loop:
        page_size, erase_size, total_size, flash_part, timeout = spi_pre_commands(
                u_boot_console, random.randint(min_f, max_f))
        spi_write_continues(u_boot_console, page_size, total_size, timeout)
        i = i + 1

def spi_erase_all(u_boot_console, total_size, timeout):
    expected_erase = 'Erased: OK'
    start = 0
    with u_boot_console.temporary_timeout(timeout):
        output = u_boot_console.run_command('sf erase 0 ' + str(hex(total_size)))
        assert expected_erase in output

@pytest.mark.buildconfigspec('cmd_sf')
def test_spi_erase_all(u_boot_console):
    min_f, max_f, loop = spi_find_freq_range(u_boot_console)
    i = 0
    while i < loop:
        page_size, erase_size, total_size, flash_part, timeout = spi_pre_commands(
                u_boot_console, random.randint(min_f, max_f))
        spi_erase_all(u_boot_console, total_size, timeout)
        i = i + 1

# Flash operations: erase/write/read
def flash_ops(
    u_boot_console, ops, start, size, offset=0, exp_ret=0, exp_str='', not_exp_str=''
):

    f = u_boot_console.config.env.get('env__spi_device_test', None)
    if not f:
        timeout = 1000000

    timeout = f.get('timeout', 1000000)

    if ops == 'erase':
        with u_boot_console.temporary_timeout(timeout):
            output = u_boot_console.run_command('sf erase %x %x' % (start, size))
    else:
        with u_boot_console.temporary_timeout(timeout):
            output = u_boot_console.run_command(
                'sf %s %x %x %x' % (ops, offset, start, size)
            )

    if exp_str:
        assert exp_str in output
    if not_exp_str:
        assert not_exp_str not in output

    ret_code = u_boot_console.run_command('echo $?')
    if exp_ret >= 0:
        assert ret_code.endswith(str(exp_ret))

    return output, ret_code

# Unlock the flash before making it fail
def qspi_unlock_exit(u_boot_console, addr, size):
    u_boot_console.run_command('sf protect unlock %x %x' % (addr, size))
    assert False, 'FAIL: Flash lock is unable to protect the data!'

# QSPI lock-unlock operations
def qspi_lock_unlock(u_boot_console, lock_addr, lock_size):
    addr = u_boot_utils.find_ram_base(u_boot_console)
    expected_erase = 'Erased: OK'
    expected_write = 'Written: OK'
    expected_erase_errors = [
        'Erase operation failed',
        'Attempted to modify a protected sector',
        'Erased: ERROR',
        'is protected and cannot be erased',
        'ERROR: flash area is locked',
    ]
    expected_write_errors = [
        'ERROR: flash area is locked',
        'Program operation failed',
        'Attempted to modify a protected sector',
        'Written: ERROR',
    ]

    # Find the protected/un-protected region
    if lock_addr < (total_size // 2):
        sect_num = (lock_addr + lock_size) // erase_size
        x = 1
        while x < sect_num:
            x *= 2
        prot_start = 0
        prot_size = x * erase_size
        unprot_start = prot_start + prot_size
        unprot_size = total_size - unprot_start
    else:
        sect_num = (total_size - lock_addr) // erase_size
        x = 1
        while x < sect_num:
            x *= 2
        prot_start = total_size - (x * erase_size)
        prot_size = total_size - prot_start
        unprot_start = 0
        unprot_size = prot_start

    # Check erase/write operation before locking
    flash_ops(u_boot_console, 'erase', prot_start, prot_size, 0, 0, expected_erase)
    flash_ops(u_boot_console, 'write', prot_start, prot_size, addr, 0, expected_write)

    # Locking the flash
    u_boot_console.run_command('sf protect lock %x %x' % (lock_addr, lock_size))
    output = u_boot_console.run_command('echo $?')
    assert output.endswith('0')

    # Check erase/write operation after locking
    output, ret_code = flash_ops(u_boot_console, 'erase', prot_start, prot_size, 0, -1)
    if not any(error in output for error in expected_erase_errors) or ret_code.endswith(
        '0'
    ):
        qspi_unlock_exit(u_boot_console, lock_addr, lock_size)

    output, ret_code = flash_ops(
        u_boot_console, 'write', prot_start, prot_size, addr, -1
    )
    if not any(error in output for error in expected_write_errors) or ret_code.endswith(
        '0'
    ):
        qspi_unlock_exit(u_boot_console, lock_addr, lock_size)

    # Check locked sectors
    sect_lock_start = random.randrange(prot_start, (prot_start + prot_size), erase_size)
    if prot_size > erase_size:
        sect_lock_size = random.randrange(
            erase_size, (prot_start + prot_size - sect_lock_start), erase_size
        )
    else:
        sect_lock_size = erase_size
    sect_write_size = random.randint(1, sect_lock_size)

    output, ret_code = flash_ops(
        u_boot_console, 'erase', sect_lock_start, sect_lock_size, 0, -1
    )
    if not any(error in output for error in expected_erase_errors) or ret_code.endswith(
        '0'
    ):
        qspi_unlock_exit(u_boot_console, lock_addr, lock_size)

    output, ret_code = flash_ops(
        u_boot_console, 'write', sect_lock_start, sect_write_size, addr, -1
    )
    if not any(error in output for error in expected_write_errors) or ret_code.endswith(
        '0'
    ):
        qspi_unlock_exit(u_boot_console, lock_addr, lock_size)

    # Check unlocked sectors
    if unprot_size != 0:
        sect_unlock_start = random.randrange(
            unprot_start, (unprot_start + unprot_size), erase_size
        )
        if unprot_size > erase_size:
            sect_unlock_size = random.randrange(
                erase_size, (unprot_start + unprot_size - sect_unlock_start), erase_size
            )
        else:
            sect_unlock_size = erase_size
        sect_write_size = random.randint(1, sect_unlock_size)

        output, ret_code = flash_ops(
            u_boot_console, 'erase', sect_unlock_start, sect_unlock_size, 0, -1
        )
        if expected_erase not in output or ret_code.endswith('1'):
            qspi_unlock_exit(u_boot_console, lock_addr, lock_size)

        output, ret_code = flash_ops(
            u_boot_console, 'write', sect_unlock_start, sect_write_size, addr, -1
        )
        if expected_write not in output or ret_code.endswith('1'):
            qspi_unlock_exit(u_boot_console, lock_addr, lock_size)

    # Unlocking the flash
    u_boot_console.run_command('sf protect unlock %x %x' % (lock_addr, lock_size))
    output = u_boot_console.run_command('echo $?')
    assert output.endswith('0')

    # Check erase/write operation after un-locking
    flash_ops(u_boot_console, 'erase', prot_start, prot_size, 0, 0, expected_erase)
    flash_ops(u_boot_console, 'write', prot_start, prot_size, addr, 0, expected_write)

    # Check previous locked sectors
    sect_lock_start = random.randrange(prot_start, (prot_start + prot_size), erase_size)
    if prot_size > erase_size:
        sect_lock_size = random.randrange(
            erase_size, (prot_start + prot_size - sect_lock_start), erase_size
        )
    else:
        sect_lock_size = erase_size
    sect_write_size = random.randint(1, sect_lock_size)

    flash_ops(
        u_boot_console, 'erase', sect_lock_start, sect_lock_size, 0, 0, expected_erase
    )
    flash_ops(
        u_boot_console,
        'write',
        sect_lock_start,
        sect_write_size,
        addr,
        0,
        expected_write,
    )

@pytest.mark.buildconfigspec('cmd_bdi')
@pytest.mark.buildconfigspec('cmd_sf')
@pytest.mark.buildconfigspec('cmd_memory')
def test_qspi_lock_unlock(u_boot_console):
    min_f, max_f, loop = spi_find_freq_range(u_boot_console)
    flashes = u_boot_console.config.env.get('env__qspi_lock_unlock', False)
    if not flashes:
        pytest.skip('No supported flash list for lock/unlock provided')

    i = 0
    while i < loop:
        page_size, erase_size, total_size, flash_part, timeout = spi_pre_commands(
                u_boot_console, random.randint(min_f, max_f))

        flashes_list = flashes.get('supported_flash', None).split(',')
        flashes_list = [x.strip() for x in flashes_list]
        if flash_part not in flashes_list:
            pytest.skip('Detected flash does not support lock/unlock')

        # For lower half of memory
        lock_addr = random.randint(0, (total_size // 2) - 1)
        lock_size = random.randint(1, ((total_size // 2) - lock_addr))
        qspi_lock_unlock(u_boot_console, lock_addr, lock_size)

        # For upper half of memory
        lock_addr = random.randint((total_size // 2), total_size - 1)
        lock_size = random.randint(1, (total_size - lock_addr))
        qspi_lock_unlock(u_boot_console, lock_addr, lock_size)

        # For entire flash
        lock_addr = random.randint(0, total_size - 1)
        lock_size = random.randint(1, (total_size - lock_addr))
        qspi_lock_unlock(u_boot_console, lock_addr, lock_size)

        i = i + 1

@pytest.mark.buildconfigspec('cmd_bdi')
@pytest.mark.buildconfigspec('cmd_sf')
@pytest.mark.buildconfigspec('cmd_memory')
def test_spi_negative(u_boot_console):
    expected_erase = 'Erased: OK'
    expected_write = 'Written: OK'
    expected_read = 'Read: OK'
    min_f, max_f, loop = spi_find_freq_range(u_boot_console)
    page_size, erase_size, total_size, flash_part, timeout = spi_pre_commands(
            u_boot_console, random.randint(min_f, max_f))
    addr = u_boot_utils.find_ram_base(u_boot_console)
    i = 0
    while i < loop:
        # Erase negative test
        start = random.randint(0, total_size)
        esize = erase_size

        # If erasesize is not multiple of flash's erase size
        while esize % erase_size == 0:
            esize = random.randint(0, total_size - start)

        error_msg = 'Erased: ERROR'
        flash_ops(
            u_boot_console, 'erase', start, esize, 0, 1, error_msg, expected_erase
        )

        # If eraseoffset exceeds beyond flash size
        eoffset = random.randint(total_size, (total_size + int(0x1000000)))
        error_msg = 'Offset exceeds device limit'
        flash_ops(
            u_boot_console, 'erase', eoffset, esize, 0, 1, error_msg, expected_erase
        )

        # If erasesize exceeds beyond flash size
        esize = random.randint((total_size - start), (total_size + int(0x1000000)))
        error_msg = 'ERROR: attempting erase past flash size'
        flash_ops(
            u_boot_console, 'erase', start, esize, 0, 1, error_msg, expected_erase
        )

        # If erase size is 0
        esize = 0
        error_msg = 'ERROR: Invalid size 0'
        flash_ops(
            u_boot_console, 'erase', start, esize, 0, 1, error_msg, expected_erase
        )

        # If erasesize is less than flash's page size
        esize = random.randint(0, page_size)
        start = random.randint(0, (total_size - page_size))
        error_msg = 'Erased: ERROR'
        flash_ops(
            u_boot_console, 'erase', start, esize, 0, 1, error_msg, expected_erase
        )

        # Write/Read negative test
        # if Write/Read size exceeds beyond flash size
        offset = random.randint(0, total_size)
        size = random.randint((total_size - offset), (total_size + int(0x1000000)))
        error_msg = 'Size exceeds partition or device limit'
        flash_ops(
            u_boot_console, 'write', offset, size, addr, 1, error_msg, expected_write
        )
        flash_ops(
            u_boot_console, 'read', offset, size, addr, 1, error_msg, expected_read
        )

        # if Write/Read offset exceeds beyond flash size
        offset = random.randint(total_size, (total_size + int(0x1000000)))
        size = random.randint(0, total_size)
        error_msg = 'Offset exceeds device limit'
        flash_ops(
            u_boot_console, 'write', offset, size, addr, 1, error_msg, expected_write
        )
        flash_ops(
            u_boot_console, 'read', offset, size, addr, 1, error_msg, expected_read
        )

        # if Write/Read size is 0
        offset = random.randint(0, 2)
        size = 0
        error_msg = 'ERROR: Invalid size 0'
        flash_ops(
            u_boot_console, 'write', offset, size, addr, 1, error_msg, expected_write
        )
        flash_ops(
            u_boot_console, 'read', offset, size, addr, 1, error_msg, expected_read
        )

        i = i + 1
