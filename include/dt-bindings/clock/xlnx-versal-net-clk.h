/* SPDX-License-Identifier: GPL-2.0 */
/*
 *  Copyright (C) 2022 Xilinx Inc.
 *
 */

#ifndef _DT_BINDINGS_CLK_VERSAL_NET_H
#define _DT_BINDINGS_CLK_VERSAL_NET_H

#define PMC_PLL					0x73
#define APU_PLL1				0x74
#define APU_PLL2				0x75
#define RPU_PLL					0x76
#define CPM_PLL					0x71
#define NOC_PLL					0x72
#define PMC_PRESRC				0x9
#define PMC_POSTCLK				0xA
#define PMC_PLL_OUT				0xB
#define PPLL					0xC
#define NOC_PRESRC				0x5
#define NOC_POSTCLK				0x6
#define NOC_PLL_OUT				0x7
#define NPLL					0x8
#define APU1_PRESRC				0xD
#define APU1_POSTCLK				0xE
#define APU1_PLL_OUT				0xF
#define APLL1					0x10
#define APU2_PRESRC				0x11
#define APU2_POSTCLK				0x12
#define APU2_PLL_OUT				0x13
#define APLL2					0x14
#define RPU_PRESRC				0x15
#define RPU_POSTCLK				0x16
#define RPU_PLL_OUT				0x17
#define RPLL					0x18
#define CPM_PRESRC				0x1
#define CPM_POSTCLK				0x2
#define CPM_PLL_OUT				0x3
#define CPLL					0x4
#define PPLL_TO_XPD				0x3F
#define NPLL_TO_XPD				0x31
#define RPLL_TO_XPD				0x6A
#define EFUSE_REF				0x36
#define SYSMON_REF				0x3A
#define IRO_SUSPEND_REF				0x41
#define USB_SUSPEND				0x2D
#define SWITCH_TIMEOUT				0x3C
#define RCLK_PMC				0x2B
#define RCLK_LPD				0x58
#define TTC0					0x80
#define TTC1					0x7F
#define TTC2					0x82
#define TTC3					0x81
#define PSM_REF					0x65
#define QSPI_REF				0x2E
#define OSPI_REF				0x23
#define SDIO0_REF				0x38
#define SDIO1_REF				0x42
#define PMC_LSBUS_REF				0x35
#define I2C_REF					0x40
#define TEST_PATTERN_REF			0x37
#define DFT_OSC_REF				0x2A
#define PMC_PL0_REF				0x34
#define PMC_PL1_REF				0x39
#define PMC_PL2_REF				0x29
#define PMC_PL3_REF				0x2F
#define CFU_REF					0x3B
#define SPARE_REF				0x3D
#define NPI_REF					0x3E
#define HSM0_REF				0x32
#define HSM1_REF				0x33
#define SD_DLL_REF				0x30
#define FPD_TOP_SWITCH				0x46
#define FPD_LSBUS				0x47
#define ACPU0					0x4A
#define ACPU1					0x4B
#define ACPU2					0x4C
#define ACPU3					0x4D
#define DBG_TRACE				0x44
#define DBG_FPD					0x49
#define LPD_TOP_SWITCH				0x6D
#define ADMA					0x6E
#define LPD_LSBUS				0x52
#define CPU_R5					0x5D
#define CPU_R5_CLUSTERB				0x60
#define CPU_R5_CLUSTERA				0x61
#define CPU_R5_OCM				0x5E
#define IOU_SWITCH				0x56
#define GEM0_REF				0x62
#define GEM1_REF				0x5A
#define GEM_TSU_REF				0x68
#define USB0_BUS_REF				0x57
#define UART0_REF				0x51
#define UART1_REF				0x70
#define SPI0_REF				0x4F
#define SPI1_REF				0x59
#define CAN0_REF				0x53
#define CAN1_REF				0x69
#define DBG_LPD					0x6F
#define TIMESTAMP_REF				0x67
#define DBG_TSTMP				0x55
#define CPM_TOPSW_REF				0x66
#define REF_CLK					0x1D
#define PL_ALT_REF_CLK				0x21

#endif
