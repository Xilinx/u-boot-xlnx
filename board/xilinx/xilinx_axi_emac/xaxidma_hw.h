/******************************************************************************
*
* (c) Copyright 2010 Xilinx, Inc. All rights reserved.
*
* This file contains confidential and proprietary information of Xilinx, Inc.
* and is protected under U.S. and international copyright and other
* intellectual property laws.
*
* DISCLAIMER
* This disclaimer is not a license and does not grant any rights to the
* materials distributed herewith. Except as otherwise provided in a valid
* license issued to you by Xilinx, and to the maximum extent permitted by
* applicable law: (1) THESE MATERIALS ARE MADE AVAILABLE "AS IS" AND WITH ALL
* FAULTS, AND XILINX HEREBY DISCLAIMS ALL WARRANTIES AND CONDITIONS, EXPRESS,
* IMPLIED, OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF
* MERCHANTABILITY, NON-INFRINGEMENT, OR FITNESS FOR ANY PARTICULAR PURPOSE;
* and (2) Xilinx shall not be liable (whether in contract or tort, including
* negligence, or under any other theory of liability) for any loss or damage
* of any kind or nature related to, arising under or in connection with these
* materials, including for any direct, or any indirect, special, incidental,
* or consequential loss or damage (including loss of data, profits, goodwill,
* or any type of loss or damage suffered as a result of any action brought by
* a third party) even if such damage or loss was reasonably foreseeable or
* Xilinx had been advised of the possibility of the same.
*
* CRITICAL APPLICATIONS
* Xilinx products are not designed or intended to be fail-safe, or for use in
* any application requiring fail-safe performance, such as life-support or
* safety devices or systems, Class III medical devices, nuclear facilities,
* applications related to the deployment of airbags, or any other applications
* that could lead to death, personal injury, or severe property or
* environmental damage (individually and collectively, "Critical
* Applications"). Customer assumes the sole risk and liability of any use of
* Xilinx products in Critical Applications, subject only to applicable laws
* and regulations governing limitations on product liability.
*
* THIS COPYRIGHT NOTICE AND DISCLAIMER MUST BE RETAINED AS PART OF THIS FILE
* AT ALL TIMES.
*
******************************************************************************/
/*****************************************************************************/
/**
 *  @file xaxidma_hw.h
 *
 * Hardware definition file. It defines the register interface and Buffer
 * Descriptor (BD) definitions.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- -------------------------------------------------------
 * 1.00a jz   05/18/10 First release
 * 2.00a jz   08/10/10 Second release, added in xaxidma_g.c, xaxidma_sinit.c,
 *                     updated tcl file, added xaxidma_porting_guide.h
 * 3.00a jz   11/22/10 Support IP core parameters change
 * </pre>
 *
 *****************************************************************************/

#ifndef XAXIDMA_HW_H_    /* prevent circular inclusions */
#define XAXIDMA_HW_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "xil_types.h"
#include "xil_io.h"

/************************** Constant Definitions *****************************/

/** @name Buffer Descriptor Alignment
 *  @{
 */
#define XAXIDMA_BD_MINIMUM_ALIGNMENT 0x40  /**< Minimum byte alignment
                                               requirement for descriptors to
                                               satisfy both hardware/software
                                               needs */
/*@}*/

/** @name Maximum transfer length
 *    This is determined by hardware
 * @{
 */
#define XAXIDMA_MAX_TRANSFER_LEN  0x7FFFFF  /* Max length hw supports */
/*@}*/

/* Register offset definitions. Register accesses are 32-bit.
 */
/** @name Device registers 
 *  Register sets on TX and RX channels are identical
 *  @{
 */
#define XAXIDMA_TX_OFFSET  0x00000000 /**< TX channel registers base offset */
#define XAXIDMA_RX_OFFSET  0x00000030 /**< RX channel registers base offset */

/* This set of registers are applicable for both channels. Add
 * XAXIDMA_TX_OFFSET to get to TX channel, and XAXIDMA_RX_OFFSET to get to RX
 * channel
 */
#define XAXIDMA_CR_OFFSET     0x00000000   /**< Channel control */
#define XAXIDMA_SR_OFFSET     0x00000004   /**< Status */
#define XAXIDMA_CDESC_OFFSET  0x00000008   /**< Current descriptor pointer */
#define XAXIDMA_TDESC_OFFSET  0x00000010   /**< Tail descriptor pointer */
/*@}*/

/** @name Bitmasks of XAXIDMA_CR_OFFSET register
 * @{
 */
#define XAXIDMA_CR_RUNSTOP_MASK	0x00000001 /**< Start/stop DMA channel */
#define XAXIDMA_CR_RESET_MASK	0x00000004 /**< Reset DMA engine */
/*@}*/

/** @name Bitmasks of XAXIDMA_SR_OFFSET register
 *
 * This register reports status of a DMA channel, including
 * run/stop/idle state, errors, and interrupts (note that interrupt
 * masks are shared with XAXIDMA_CR_OFFSET register, and are defined
 * in the _IRQ_ section.
 *
 * The interrupt coalescing threshold value and delay counter value are
 * also shared with XAXIDMA_CR_OFFSET register, and are defined in a
 * later section.  
 * @{
 */
#define XAXIDMA_HALTED_MASK         0x00000001  /**< DMA channel halted */
#define XAXIDMA_IDLE_MASK           0x00000002  /**< DMA channel idle */
#define XAXIDMA_ERR_INTERNAL_MASK   0x00000010  /**< Datamover internal err */
#define XAXIDMA_ERR_SLAVE_MASK      0x00000020  /**< Datamover slave err */
#define XAXIDMA_ERR_DECODE_MASK     0x00000040  /**< Datamover decode err */
#define XAXIDMA_ERR_SG_INT_MASK     0x00000100  /**< SG internal err */
#define XAXIDMA_ERR_SG_SLV_MASK     0x00000200  /**< SG slave err */
#define XAXIDMA_ERR_SG_DEC_MASK     0x00000400  /**< SG decode err */
#define XAXIDMA_ERR_ALL_MASK        0x00000770  /**< All errors */

/** @name Bitmask for interrupts
 * These masks are shared by XAXIDMA_CR_OFFSET register and 
 * XAXIDMA_SR_OFFSET register
 * @{
 */
#define XAXIDMA_IRQ_IOC_MASK    0x00001000 /**< Completion intr */
#define XAXIDMA_IRQ_DELAY_MASK  0x00002000 /**< Delay interrupt */
#define XAXIDMA_IRQ_ERROR_MASK  0x00004000 /**< Error interrupt */
#define XAXIDMA_IRQ_ALL_MASK    0x00007000 /**< All interrupts */
/*@}*/

/** @name Bitmask and shift for delay and coalesce
 * These masks are shared by XAXIDMA_CR_OFFSET register and 
 * XAXIDMA_SR_OFFSET register
 * @{
 */
#define XAXIDMA_DELAY_MASK      0xFF000000 /**< Delay timeout counter */
#define XAXIDMA_COALESCE_MASK   0x00FF0000 /**< Coalesce counter */

#define XAXIDMA_DELAY_SHIFT     24
#define XAXIDMA_COALESCE_SHIFT  16
/*@}*/


/* Buffer Descriptor (BD) definitions
 */

/** @name Buffer Descriptor offsets
 *  USR* fields are defined by higher level IP.
 *  setup for EMAC type devices. The first 13 words are used by hardware.
 *  All words after the 13rd word are for software use only.
 *  @{
 */
#define XAXIDMA_BD_NDESC_OFFSET        0x00  /**< Next descriptor pointer */
#define XAXIDMA_BD_BUFA_OFFSET         0x08  /**< Buffer address */
#define XAXIDMA_BD_CTRL_LEN_OFFSET     0x18  /**< Control/buffer length */
#define XAXIDMA_BD_STS_OFFSET          0x1C  /**< Status */
#define XAXIDMA_BD_USR0_OFFSET         0x20  /**< User IP specific word0 */
#define XAXIDMA_BD_USR1_OFFSET         0x24  /**< User IP specific word1 */
#define XAXIDMA_BD_USR2_OFFSET         0x28  /**< User IP specific word2 */
#define XAXIDMA_BD_USR3_OFFSET         0x2C  /**< User IP specific word3 */
#define XAXIDMA_BD_USR4_OFFSET         0x30  /**< User IP specific word4 */
#define XAXIDMA_BD_ID_OFFSET           0x34  /**< Sw ID */
#define XAXIDMA_BD_HAS_STSCNTRL_OFFSET 0x38  /**< Whether has stscntrl strm */	
#define XAXIDMA_BD_HAS_DRE_OFFSET      0x3C  /**< Whether has DRE */	

#define XAXIDMA_BD_HAS_DRE_MASK        0xF00 /**< Whether has DRE mask */	
#define XAXIDMA_BD_WORDLEN_MASK        0xFF  /**< Whether has DRE mask */	

#define XAXIDMA_BD_HAS_DRE_SHIFT       8     /**< Whether has DRE shift */	
#define XAXIDMA_BD_WORDLEN_SHIFT       0     /**< Whether has DRE shift */	

#define XAXIDMA_BD_START_CLEAR    8   /**< Offset to start clear */
#define XAXIDMA_BD_BYTES_TO_CLEAR 48  /**< BD specific bytes to be cleared */

#define XAXIDMA_BD_NUM_WORDS      16  /**< Total number of words for one BD*/
#define XAXIDMA_BD_HW_NUM_BYTES   52  /**< Number of bytes hw used */

/* The offset of the last app word.
 */
#define XAXIDMA_LAST_APPWORD	4

/*@}*/


/** @name Bitmasks of XAXIDMA_BD_CTRL_OFFSET register
 *  @{
 */
#define XAXIDMA_BD_CTRL_LENGTH_MASK 0x007FFFFF /**< Requested len */
#define XAXIDMA_BD_CTRL_TXSOF_MASK 0x08000000 /**< First tx packet */
#define XAXIDMA_BD_CTRL_TXEOF_MASK 0x04000000 /**< Last tx packet */
#define XAXIDMA_BD_CTRL_ALL_MASK   0x0C000000 /**< All control bits */ 
/*@}*/

/** @name Bitmasks of XAXIDMA_BD_STS_OFFSET register
 *  @{
 */
#define XAXIDMA_BD_STS_ACTUAL_LEN_MASK 0x007FFFFF /**< Actual len */
#define XAXIDMA_BD_STS_COMPLETE_MASK   0x80000000 /**< Completed */
#define XAXIDMA_BD_STS_DEC_ERR_MASK    0x40000000 /**< Decode error */
#define XAXIDMA_BD_STS_SLV_ERR_MASK    0x20000000 /**< Slave error */
#define XAXIDMA_BD_STS_INT_ERR_MASK    0x10000000 /**< Internal err */
#define XAXIDMA_BD_STS_ALL_ERR_MASK    0x70000000 /**< All errors */
#define XAXIDMA_BD_STS_RXSOF_MASK      0x08000000 /**< First rx pkt */
#define XAXIDMA_BD_STS_RXEOF_MASK      0x04000000 /**< Last rx pkt */
#define XAXIDMA_BD_STS_ALL_MASK      0xFC000000 /**< All status bits */
/*@}*/

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

#define XAxiDma_In32  Xil_In32 
#define XAxiDma_Out32 Xil_Out32

/*****************************************************************************/
/**
*
* Read the given register.
*
* @param    BaseAddress is the base address of the device
* @param    RegOffset is the register offset to be read
*
* @return   The 32-bit value of the register
*
* @note
* C-style signature:
*    u32 XAxiDma_ReadReg(u32 BaseAddress, u32 RegOffset)
*
******************************************************************************/
#define XAxiDma_ReadReg(BaseAddress, RegOffset)             \
    XAxiDma_In32((BaseAddress) + (RegOffset))

/*****************************************************************************/
/**
*
* Write the given register.
*
* @param    BaseAddress is the base address of the device
* @param    RegOffset is the register offset to be written
* @param    Data is the 32-bit value to write to the register
*
* @return   None.
*
* @note
* C-style signature:
*    void XAxiDma_WriteReg(u32 BaseAddress, u32 RegOffset, u32 Data)
*
******************************************************************************/
#define XAxiDma_WriteReg(BaseAddress, RegOffset, Data)          \
    XAxiDma_Out32((BaseAddress) + (RegOffset), (Data))

#ifdef __cplusplus
}
#endif

#endif
