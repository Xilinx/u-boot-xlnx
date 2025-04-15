/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022, Xilinx, Inc.
 * Copyright (C) 2022 - 2025, Advanced Micro Devices, Inc.
 */

#ifndef _DT_BINDINGS_VERSAL2_RESETS_H
#define _DT_BINDINGS_VERSAL2_RESETS_H

#include <dt-bindings/reset/xlnx-versal-net-resets.h>

#define VERSAL_RST_UFS			(0xC104103U)
#define VERSAL_RST_UFS_PHY		(0xC10407CU)
#define VERSAL_RST_UDH_DRD		(0xC10407BU)
#define VERSAL_RST_DC			(0xC104119U)
#define VERSAL_RST_GPU			(0xC10411AU)
#define VERSAL_RST_GPU_RECOV		(0xC10411BU)
#define VERSAL_RST_UDH_AUX		(0xC10411DU)
#define VERSAL_RST_UDH_DP		(0xC10411EU)
#define VERSAL_RST_UDH_HDCPRAM		(0xC10411FU)

#endif
