.. SPDX-License-Identifier: GPL-2.0+

Boot Count Limit
================

This is enabled by CONFIG_BOOTCOUNT_LIMIT.

This allows to detect multiple failed attempts to boot Linux.

After a power-on reset, the ``bootcount`` variable will be initialized to 1, and
each reboot will increment the value by 1.

If, after a reboot, the new value of ``bootcount`` exceeds the value of
``bootlimit``, then instead of the standard boot action (executing the contents
of ``bootcmd``), an alternate boot action will be performed, and the contents of
``altbootcmd`` will be executed.

If the variable ``bootlimit`` is not defined in the environment, the Boot Count
Limit feature is disabled. If it is enabled, but ``altbootcmd`` is not defined,
then U-Boot will drop into interactive mode and remain there.

It is the responsibility of some application code (typically a Linux
application) to reset the variable ``bootcount`` to 0 when the system booted
successfully, thus allowing for more boot cycles.

CONFIG_BOOTCOUNT_FS
--------------------

This adds support for maintaining boot count in a file on a filesystem.
Tested filesystems are FAT and EXT. The file to use is defined by:

CONFIG_SYS_BOOTCOUNT_FS_INTERFACE
CONFIG_SYS_BOOTCOUNT_FS_DEVPART
CONFIG_SYS_BOOTCOUNT_FS_NAME

The format of the file is:

.. list-table::
   :header-rows: 1

   * - type
     - entry
   * - u8
     - magic
   * - u8
     - version
   * - u8
     - bootcount
   * - u8
     - upgrade_available

To prevent unintended usage of ``altbootcmd``, the ``upgrade_available``
variable is used.
If ``upgrade_available`` is 0, ``bootcount`` is not saved.
If ``upgrade_available`` is 1, ``bootcount`` is saved.
So a userspace application should take care of setting the ``upgrade_available``
and ``bootcount`` variables to 0, if the system boots successfully.
This also avoids writing the ``bootcount`` information on all reboots.
