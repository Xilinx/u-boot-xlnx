/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 - 2022, Xilinx, Inc.
 * Copyright (C) 2022, Advanced Micro Devices, Inc.
 */

#include <linux/build_bug.h>

void mem_map_fill(void);
/* EL3 clock/timer register setup, called from board_early_init_r() */
void versal_net_timer_setup(void);
