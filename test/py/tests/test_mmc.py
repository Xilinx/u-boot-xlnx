#
# Copyright (c) 2016 Michal Simek
#
# SPDX-License-Identifier: GPL-2.0

import pytest
import random
import re
import u_boot_utils

mmc_set_up = False
controllers = 0
devices = {}

@pytest.mark.buildconfigspec("cmd_mmc")
def test_mmc_list(u_boot_console):
    output = u_boot_console.run_command("mmc list")
    if "No MMC device available" in output:
        pytest.skip('No SD/MMC/eMMC controller available')

    array = output.split( )

    global devices
    global controllers
    controllers = len(array) / 2
    for x in range(0, controllers):
        y = x * 2
        devices[x] = {}
        devices[x]["name"] = array[y]

    global mmc_set_up
    mmc_set_up = True

@pytest.mark.buildconfigspec("cmd_mmc")
def test_mmc_dev(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    fail = 0
    for x in range(0, controllers):
        output = u_boot_console.run_command('mmc dev %d' % x)

        # Some sort of switch here
        if "Card did not respond to voltage select" in output:
            fail = 1
            devices[x]["detected"] = "no"
        else:
            devices[x]["detected"] = "yes"

    if fail:
        pytest.fail("Card not present")

@pytest.mark.buildconfigspec("cmd_mmc")
def test_mmcinfo(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            output = u_boot_console.run_command("mmcinfo")
            if "busy timeout" in output:
                  pytest.skip('No SD/MMC/eMMC device present')

            obj = re.search(r'Capacity: (\d+|\d+[\.]?\d)', output)
            try:
                capacity = float(obj.groups()[0])
                print capacity
                devices[x]["capacity"] = capacity
                print ("Capacity of dev %d is: %g GiB" % (x, capacity))
            except ValueError:
                pytest.fail("MMC capacity not recognized")

@pytest.mark.buildconfigspec("cmd_mmc")
def test_mmc_info(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)

            output = u_boot_console.run_command("mmc info")

            obj = re.search(r'Capacity: (\d+|\d+[\.]?\d)', output)
            try:
                capacity = float(obj.groups()[0])
                print capacity
                if devices[x]["capacity"] != capacity:
                      pytest.fail("MMC capacity doesn't match mmcinfo")

            except ValueError:
                pytest.fail("MMC capacity not recognized")

@pytest.mark.buildconfigspec("cmd_mmc")
def test_mmc_rescan(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            output = u_boot_console.run_command("mmc rescan")
            # Not sure if this can be any error
            if output:
                pytest.fail("mmc rescan has something to check")


@pytest.mark.buildconfigspec("cmd_mmc")
def test_mmc_part(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            output = u_boot_console.run_command("mmc part")

            lines = output.split("\n")
            part_fat = []
            part_ext = []
            for line in lines:
                obj = re.search(r'(\d)\s+\d+\s+\d+\s+\w+\d+\w+-\d+\s+(\d+\w+)', line)
                if obj:
                    part_id = int(obj.groups()[0])
                    part_type = obj.groups()[1]
                    print ("part_id:%d, part_type:%s" % (part_id, part_type))

                    if part_type == '0c' or part_type == '0b' or part_type == '0e':
                        print "Fat detected"
                        part_fat.append(part_id)
                    elif part_type == '83':
                        print "ext detected"
                        part_ext.append(part_id)
                    else:
                        pytest.fail("Unsupported Filesystem on device %d" % x)
            devices[x]["ext4"] = part_ext
            devices[x]["ext2"] = part_ext
            devices[x]["fat"] = part_fat

            if not part_ext and not part_fat:
                pytest.fail("No partition detected on device %d" %x)

@pytest.mark.buildconfigspec("cmd_mmc")
@pytest.mark.buildconfigspec("cmd_fat")
def test_mmc_fatls_fatinfo(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    part_detect = 0
    fs = "fat"
    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            try:
                partitions = devices[x][fs]
            except:
                print ("No %s table on this device" % fs.upper());
                continue

            for part in partitions:
                output = u_boot_console.run_command("fatls mmc %d:%s" % (x, part))
                if "Unrecognized filesystem type" in output:
                        partitions.remove(part)
                        pytest.fail("Unrecognized filesystem")

                if not re.search("\d file\(s\), \d dir\(s\)", output):
                    pytest.fail("%s read failed on device %d" % (fs.upper, x))
                # Maybe separate this usecase
                # Also maybe detect not readable chars
                output = u_boot_console.run_command("fatinfo mmc %d:%s" % (x, part))
                string = "Filesystem: %s" % fs.upper
                if re.search(string, output):
                    pytest.fail("%s FS failed on device %d" % (fs.upper(), x))
                part_detect = 1

    if not part_detect:
        pytest.skip("No %s partition detected" % fs.upper())

@pytest.mark.buildconfigspec("cmd_mmc")
@pytest.mark.buildconfigspec("cmd_fat")
@pytest.mark.buildconfigspec('cmd_memory')
def test_mmc_fatload_fatwrite(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    part_detect = 0
    fs = "fat"
    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            try:
                partitions = devices[x][fs]
            except:
                print ("No %s table on this device" % fs.upper());
                continue

            for part in partitions:
                part_detect = 1
                addr = u_boot_utils.find_ram_base(u_boot_console)
                devices[x]["addr_%d" % part] = addr
                size = random.randint(4, 1 * 1024 * 1024)
                devices[x]["size_%d" % part] = size
                # count CRC32
                output = u_boot_console.run_command('crc32 %x %x' % (addr, size))
                m = re.search('==> (.+?)', output)
                if not m:
                    pytest.fail("CRC32 failed")
                expected_crc32 = m.group(1)
                devices[x]["expected_crc32_%d" % part] = expected_crc32
                # do write

                file = '%s_%d' % ("uboot_test", size)
                devices[x]["file_%d" % part] = file
                output = u_boot_console.run_command("%swrite mmc %d:%s %x %s %x" % (fs, x, part, addr, file, size))
                assert "Unable to write" not in output
                assert "Error" not in output
                assert "overflow" not in output
                expected_text = "%d bytes written" % size
                assert expected_text in output

                alignment = int(u_boot_console.config.buildconfig.get('config_sys_cacheline_size', 128))
                offset = random.randrange(alignment, 1024, alignment)
                output = u_boot_console.run_command("%sload mmc %d:%s %x %s" % (fs, x, part, addr + offset, file))
                assert "Invalid FAT entry" not in output
                assert "Unable to read file" not in output
                assert "Misaligned buffer address" not in output
                expected_text = "%d bytes read" % size
                assert expected_text in output

                output = u_boot_console.run_command('crc32 %x $filesize' % (addr + offset))
                assert expected_crc32 in output

    if not part_detect:
        pytest.skip("No %s partition detected" % fs.upper())

@pytest.mark.buildconfigspec("cmd_mmc")
@pytest.mark.buildconfigspec("cmd_ext4")
def test_mmc_ext4ls(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    part_detect = 0
    fs = "ext4"
    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            try:
                partitions = devices[x][fs]
            except:
                print ("No %s table on this device" % fs.upper());
                continue

            u_boot_console.run_command('mmc dev %d' % x)
            for part in partitions:
                output = u_boot_console.run_command("%sls mmc %d:%s" % (fs, x, part))
                if "Unrecognized filesystem type" in output:
                        partitions.remove(part)
                        pytest.fail("Unrecognized filesystem")
                part_detect = 1

    if not part_detect:
        pytest.skip("No %s partition detected" % fs.upper())

@pytest.mark.buildconfigspec("cmd_mmc")
@pytest.mark.buildconfigspec("cmd_ext4")
@pytest.mark.buildconfigspec("ext4_write")
@pytest.mark.buildconfigspec('cmd_memory')
def test_mmc_ext4load_ext4write(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    part_detect = 0
    fs = "ext4"
    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            try:
                partitions = devices[x][fs]
            except:
                print ("No %s table on this device" % fs.upper());
                continue

            for part in partitions:
                part_detect = 1
                addr = u_boot_utils.find_ram_base(u_boot_console)
                devices[x]["addr_%d" % part] = addr
                size = random.randint(4, 1 * 1024 * 1024)
                devices[x]["size_%d" % part] = size
                # count CRC32
                output = u_boot_console.run_command('crc32 %x %x' % (addr, size))
                m = re.search('==> (.+?)', output)
                if not m:
                    pytest.fail("CRC32 failed")
                expected_crc32 = m.group(1)
                devices[x]["expected_crc32_%d" % part] = expected_crc32
                # do write

                file = '%s_%d' % ("uboot_test", size)
                devices[x]["file_%d" % part] = file
                output = u_boot_console.run_command("%swrite mmc %d:%s %x /%s %x" % (fs, x, part, addr, file, size))
                assert "Unable to write" not in output
                assert "Error" not in output
                # maybe not valid for ext4
                assert "overflow" not in output
                expected_text = "%d bytes written" % size
                assert expected_text in output

                # Read it back with 64 aligned offset
                # Todo read ALIGN
                offset = random.randrange(128, 1024, 128)
                output = u_boot_console.run_command("%sload mmc %d:%s %x /%s" % (fs, x, part, addr + offset, file))
                expected_text = "%d bytes read" % size
                assert expected_text in output

                output = u_boot_console.run_command('crc32 %x $filesize' % (addr + offset))
                assert expected_crc32 in output

    if not part_detect:
        pytest.skip("No %s partition detected" % fs.upper())

@pytest.mark.buildconfigspec("cmd_mmc")
@pytest.mark.buildconfigspec("cmd_ext2")
def test_mmc_ext2ls(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    part_detect = 0
    fs = "ext2"
    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            try:
                partitions = devices[x][fs]
            except:
                print ("No %s table on this device" % fs.upper());
                continue

            for part in partitions:
                part_detect = 1
                output = u_boot_console.run_command("%sls mmc %d:%s" % (fs, x, part))
                if "Unrecognized filesystem type" in output:
                       partitions.remove(part)
                       pytest.fail("Unrecognized filesystem")
                part_detect = 1

    if not part_detect:
        pytest.skip("No %s partition detected" % fs.upper())

@pytest.mark.buildconfigspec("cmd_mmc")
@pytest.mark.buildconfigspec("cmd_ext2")
@pytest.mark.buildconfigspec("cmd_ext4")
@pytest.mark.buildconfigspec("ext4_write")
@pytest.mark.buildconfigspec('cmd_memory')
def test_mmc_ext2load(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    part_detect = 0
    fs = "ext2"
    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            try:
                partitions = devices[x][fs]
            except:
                print ("No %s table on this device" % fs.upper());
                continue

            for part in partitions:
                part_detect = 1
                # Have to share information about ext4 file which was written
                # Maybe just choose whatever available file
                addr = devices[x]["addr_%d" % part]
                size = devices[x]["size_%d" % part]
                expected_crc32 = devices[x]["expected_crc32_%d" % part]
                file = devices[x]["file_%d" % part]

                # Read it back with 64 aligned offset
                # Todo read ALIGN
                offset = random.randrange(128, 1024, 128)
                output = u_boot_console.run_command("%sload mmc %d:%s %x /%s" % (fs, x, part, addr + offset, file))
                expected_text = "%d bytes read" % size
                assert expected_text in output

                output = u_boot_console.run_command('crc32 %x $filesize' % (addr + offset))
                assert expected_crc32 in output

    if not part_detect:
        pytest.skip("No %s partition detected" % fs.upper())

@pytest.mark.buildconfigspec("cmd_mmc")
@pytest.mark.buildconfigspec("cmd_fs_generic")
def test_mmc_ls(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    part_detect = 0
    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            for fs in ['fat', 'ext4']:
                try:
                    partitions = devices[x][fs]
                except:
                    print ("No %s table on this device" % fs.upper());
                    continue

                for part in partitions:
                    part_detect = 1
                    output = u_boot_console.run_command("ls mmc %d:%s" % (x, part))
                    if re.search("No \w+ table on this device", output):
                        pytest.fail("%s: Partition table not found %d" % (fs.upper(), x))

    if not part_detect:
        pytest.skip("No partition detected")

@pytest.mark.buildconfigspec("cmd_mmc")
@pytest.mark.buildconfigspec("cmd_fs_generic")
def test_mmc_load(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    part_detect = 0
    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            for fs in ['fat', 'ext4']:
                try:
                    partitions = devices[x][fs]
                except:
                    print ("No %s table on this device" % fs.upper());
                    continue

                for part in partitions:
                    part_detect = 1
                    # Have to share information about ext4 file which was written
                    # Maybe just choose whatever available file
                    addr = devices[x]["addr_%d" % part]
                    size = devices[x]["size_%d" % part]
                    expected_crc32 = devices[x]["expected_crc32_%d" % part]
                    file = devices[x]["file_%d" % part]

                    # Read it back with 64 aligned offset
                    # Todo read ALIGN
                    offset = random.randrange(128, 1024, 128)
                    output = u_boot_console.run_command("load mmc %d:%s %x /%s" % (x, part, addr + offset, file))
                    expected_text = "%d bytes read" % size
                    assert expected_text in output

                    output = u_boot_console.run_command('crc32 %x $filesize' % (addr + offset))
                    assert expected_crc32 in output

    if not part_detect:
        pytest.skip("No partition detected")

@pytest.mark.buildconfigspec("cmd_mmc")
@pytest.mark.buildconfigspec("cmd_fs_generic")
def test_mmc_save(u_boot_console):
    if not mmc_set_up:
        pytest.skip('No SD/MMC/eMMC controller available')

    if not devices:
        pytest.skip('No devices detected')

    part_detect = 0
    for x in range(0, controllers):
        if devices[x]["detected"] == "yes":
            u_boot_console.run_command('mmc dev %d' % x)
            for fs in ['fat', 'ext4']:
                try:
                    partitions = devices[x][fs]
                except:
                    print ("No %s table on this device" % fs.upper());
                    continue

                for part in partitions:
                    part_detect = 1
                    addr = devices[x]["addr_%d" % part]
                    # Delete file not to have too many files on cards
                    size = 0
                    file = devices[x]["file_%d" % part]

                    # Read it back with 64 aligned offset
                    # Todo read ALIGN
                    offset = random.randrange(128, 1024, 128)
                    output = u_boot_console.run_command("save mmc %d:%s %x /%s %d" % (x, part, addr + offset, file, size))
                    expected_text = "%d bytes written" % size
                    assert expected_text in output

    if not part_detect:
        pytest.skip("No partition detected")
