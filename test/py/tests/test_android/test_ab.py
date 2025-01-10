# SPDX-License-Identifier: GPL-2.0
# (C) Copyright 2018 Texas Instruments, <www.ti.com>

# Test A/B update commands.

import os
import pytest
import u_boot_utils

class ABTestDiskImage(object):
    """Disk Image used by the A/B tests."""

    def __init__(self, u_boot_console):
        """Initialize a new ABTestDiskImage object.

        Args:
            u_boot_console: A U-Boot console.

        Returns:
            Nothing.
        """

        filename = 'test_ab_disk_image.bin'

        persistent = u_boot_console.config.persistent_data_dir + '/' + filename
        self.path = u_boot_console.config.result_dir  + '/' + filename

        with u_boot_utils.persistent_file_helper(u_boot_console.log, persistent):
            if os.path.exists(persistent):
                u_boot_console.log.action('Disk image file ' + persistent +
                    ' already exists')
            else:
                u_boot_console.log.action('Generating ' + persistent)
                fd = os.open(persistent, os.O_RDWR | os.O_CREAT)
                os.ftruncate(fd, 524288)
                os.close(fd)
                cmd = ('sgdisk', persistent)
                u_boot_utils.run_and_log(u_boot_console, cmd)

                cmd = ('sgdisk', '--new=1:64:512', '--change-name=1:misc',
                    persistent)
                u_boot_utils.run_and_log(u_boot_console, cmd)
                cmd = ('sgdisk', '--load-backup=' + persistent)
                u_boot_utils.run_and_log(u_boot_console, cmd)

        cmd = ('cp', persistent, self.path)
        u_boot_utils.run_and_log(u_boot_console, cmd)

di = None
@pytest.fixture(scope='function')
def ab_disk_image(u_boot_console):
    global di
    if not di:
        di = ABTestDiskImage(u_boot_console)
    return di

def ab_dump(u_boot_console, slot_num, crc):
    output = u_boot_console.run_command('bcb ab_dump host 0#misc')
    header, slot0, slot1 = output.split('\r\r\n\r\r\n')
    slots = [slot0, slot1]
    slot_suffixes = ['_a', '_b']

    header = dict(map(lambda x: map(str.strip, x.split(':')), header.split('\r\r\n')))
    assert header['Bootloader Control'] == '[misc]'
    assert header['Active Slot'] == slot_suffixes[slot_num]
    assert header['Magic Number'] == '0x42414342'
    assert header['Version'] == '1'
    assert header['Number of Slots'] == '2'
    assert header['Recovery Tries Remaining'] == '0'
    assert header['CRC'] == '{} (Valid)'.format(crc)

    slot = dict(map(lambda x: map(str.strip, x.split(':')), slots[slot_num].split('\r\r\n\t- ')[1:]))
    assert slot['Priority'] == '15'
    assert slot['Tries Remaining'] == '6'
    assert slot['Successful Boot'] == '0'
    assert slot['Verity Corrupted'] == '0'

@pytest.mark.boardspec('sandbox')
@pytest.mark.buildconfigspec('android_ab')
@pytest.mark.buildconfigspec('cmd_bcb')
@pytest.mark.requiredtool('sgdisk')
def test_ab(ab_disk_image, u_boot_console):
    """Test the 'bcb ab_select' command."""

    u_boot_console.run_command('host bind 0 ' + ab_disk_image.path)

    output = u_boot_console.run_command('bcb ab_select slot_name host 0#misc')
    assert 're-initializing A/B metadata' in output
    assert 'Attempting slot a, tries remaining 7' in output
    output = u_boot_console.run_command('printenv slot_name')
    assert 'slot_name=a' in output
    ab_dump(u_boot_console, 0, '0xd438d1b9')

    output = u_boot_console.run_command('bcb ab_select slot_name host 0:1')
    assert 'Attempting slot b, tries remaining 7' in output
    output = u_boot_console.run_command('printenv slot_name')
    assert 'slot_name=b' in output
    ab_dump(u_boot_console, 1, '0x011ec016')
