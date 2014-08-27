#!/bin/bash

TOOLCHAIN_LOC=$HOME/toolchains/codesourcery-xilinx-linux/bin


export CROSS_COMPILE=arm-xilinx-linux-gnueabi-

export PATH=$PATH:$TOOLCHAIN_LOC:$MKIMAGE_LOC

make -j 10 zynq_av108

