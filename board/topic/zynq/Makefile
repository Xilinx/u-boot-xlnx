# SPDX-License-Identifier: GPL-2.0+

obj-y	:= board.o
obj-y	+= ../../xilinx/common/board.o

# Remove quotes
hw-platform-y :=$(shell echo $(CONFIG_DEFAULT_DEVICE_TREE))

obj-$(CONFIG_XPL_BUILD) += $(hw-platform-y)/ps7_init_gpl.o
