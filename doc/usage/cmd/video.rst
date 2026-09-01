.. SPDX-License-Identifier: GPL-2.0

.. index::
   single: video (command)

video command
=============

Synopsis
--------

::

    video vidinfo
    video dispbars

Description
-----------

The video command provides test/debug subcommands for the active video
device:

vidinfo
~~~~~~~

Prints generic information about the active video device: device name,
resolution, bits per pixel, frame buffer line length, and frame buffer
address/size.

dispbars
~~~~~~~~

Fills the active video device's framebuffer with a fixed set of vertical
colour bars (white, red, green, blue, yellow, black) and syncs the
display. It is useful for exercising a display pipeline (controller,
DMA, output stage) end to end without needing to transfer a BMP file
first.

Both subcommands operate on the first probed UCLASS_VIDEO device, the
same as the bmp and cls commands.

Configuration
-------------

The video command is only available if CONFIG_CMD_VIDEO_TEST=y.
