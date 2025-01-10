// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2017 Andes Technology Corporation
 * Rick Chen, Andes Technology Corporation <rick@andestech.com>
 */

#include <linux/types.h>
#include <asm/u-boot-riscv.h>

unsigned long do_go_exec(ulong (*entry)(int, char * const []),
			 int argc, char *const argv[])
{
	cleanup_before_linux();

	return entry(argc, argv);
}
