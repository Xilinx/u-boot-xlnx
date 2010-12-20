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
 *
 * @file xaxidma_bd.h
 *
 * Buffer descriptor (BD) management API.
 *
 * <b> Buffer Descriptors </b>
 *
 * A BD defines a DMA transaction (see "Transaction" section in xaxidma.h).
 * All accesses to a BD go through this set of API.
 *
 * The XAxiDma_Bd structure defines a BD. The first XAXIDMA_BD_HW_NUM_BYTES
 * are shared between hardware and software.
 *
 * <b> Actual Transfer Length </b>
 *
 * The actual transfer length for receive could be smaller than the requested
 * transfer length. The hardware sets the actual transfer length in the 
 * completed BD. The API to retrieve the actual transfer length is
 * XAxiDma_GetActualLength().
 *
 * <b> User IP words </b>
 *
 * There are 5 user IP words in each BD. 
 *
 * If hardware does not have the StsCntrl stream built in, then these words
 * are not usable. Retrieving these words get a NULL pointer and setting 
 * these words results an error.
 *
 * <b> Performance </b>
 *
 * BDs are typically in a non-cached memory space. Reducing the number of
 * I/O operations to BDs can improve overall performance of the DMA channel.
 *
 * <pre>
 * MODIFICATION HISTORY:
 *
 * Ver   Who  Date     Changes
 * ----- ---- -------- ------------------------------------------------------
 * 1.00a jz   05/18/10 First release
 * 2.00a jz   08/10/10 Second release, added in xaxidma_g.c, xaxidma_sinit.c,
 *                     updated tcl file, added xaxidma_porting_guide.h
 * 3.00a jz   11/22/10 Support IP core parameters change
 * </pre>
 *****************************************************************************/

#ifndef XAXIDMA_BD_H_    /* To prevent circular inclusions */
#define XAXIDMA_BD_H_

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
//#include "xenv.h"	/* for memset */
#include "xaxidma_hw.h"
#include "xstatus.h"
#include "xdebug.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/**
 * The XAxiDma_Bd is the type for a buffer descriptor (BD).
 */
typedef u32 XAxiDma_Bd[XAXIDMA_BD_NUM_WORDS];


/***************** Macros (Inline Functions) Definitions *********************/

/******************************************************************************
 * Define methods to flush and invalidate cache for BDs should they be
 * located in cached memory. These macros may NOPs if the underlying
 * XCACHE_FLUSH_DCACHE_RANGE and XCACHE_INVALIDATE_DCACHE_RANGE macros are not
 * implemented or they do nothing.
 *****************************************************************************/
#ifdef XCACHE_FLUSH_DCACHE_RANGE
#  define XAXIDMA_CACHE_FLUSH(BdPtr)               \
      XCACHE_FLUSH_DCACHE_RANGE((BdPtr), XAXIDMA_BD_HW_NUM_BYTES)
#else
#  define XAXIDMA_CACHE_FLUSH(BdPtr)
#endif

#ifdef XCACHE_INVALIDATE_DCACHE_RANGE
#  define XAXIDMA_CACHE_INVALIDATE(BdPtr)          \
      XCACHE_INVALIDATE_DCACHE_RANGE((BdPtr), XAXIDMA_BD_HW_NUM_BYTES)
#else
#  define XAXIDMA_CACHE_INVALIDATE(BdPtr)
#endif

/*****************************************************************************/
/**
*
* Read the given Buffer Descriptor word.
*
* @param    BaseAddress is the base address of the BD to read
* @param    Offset is the word offset to be read
*
* @return   The 32-bit value of the field
*
* @note
* C-style signature:
*    u32 XAxiDma_BdRead(u32 BaseAddress, u32 Offset)
*
******************************************************************************/
#define XAxiDma_BdRead(BaseAddress, Offset)				\
	(*(u32*)((u32)(BaseAddress) + (u32)(Offset)))


/*****************************************************************************/
/**
*
* Write the given Buffer Descriptor word.
*
* @param    BaseAddress is the base address of the BD to write
* @param    Offset is the word offset to be written
* @param    Data is the 32-bit value to write to the field
*
* @return   None.
*
* @note
* C-style signature:
*    void XAxiDma_BdWrite(u32 BaseAddress, u32 RegOffset, u32 Data)
*
******************************************************************************/
#define XAxiDma_BdWrite(BaseAddress, Offset, Data)			\
	(*(u32*)((u32)(BaseAddress) + (u32)(Offset)) = (Data))


/*****************************************************************************/
/**
 * Zero out BD specific fields. BD fields that are for the BD ring or for the
 * system hardware build information are not touched.
 *
 * @param  BdPtr is the BD to operate on
 *
 * @return Nothing
 *
 * @note
 * C-style signature:
 *    void XAxiDma_BdClear(XAxiDma_Bd* BdPtr)
 *
 *****************************************************************************/
#define XAxiDma_BdClear(BdPtr)                    \
  memset((void *)(((u32)(BdPtr)) + XAXIDMA_BD_START_CLEAR), 0, \
    XAXIDMA_BD_BYTES_TO_CLEAR)

/*****************************************************************************/
/**
 * Get the control bits for the BD
 * 
 * @param  BdPtr is the BD to operate on
 *
 * @return
 *  The bit mask for the control of the BD
 *
 * @note
 * C-style signature:
 *    u32 XAxiDma_BdGetCtrl(XAxiDma_Bd* BdPtr)
 *
 *****************************************************************************/
#define XAxiDma_BdGetCtrl(BdPtr)				\
		(XAxiDma_BdRead((BdPtr), XAXIDMA_BD_CTRL_LEN_OFFSET)	\
		& XAXIDMA_BD_CTRL_ALL_MASK)

/*****************************************************************************/
/**
 * Retrieve the status of a BD
 *
 * @param  BdPtr is the BD to operate on
 *
 * @return Word at offset XAXIDMA_BD_DMASR_OFFSET. Use XAXIDMA_BD_STS_***
 *         values defined in xaxidma_hw.h to interpret the returned value
 *
 * @note
 * C-style signature:
 *    u32 XAxiDma_BdGetSts(XAxiDma_Bd* BdPtr)
 *
 *****************************************************************************/
#define XAxiDma_BdGetSts(BdPtr)              \
	(XAxiDma_BdRead((BdPtr), XAXIDMA_BD_STS_OFFSET) & \
		XAXIDMA_BD_STS_ALL_MASK)

/*****************************************************************************/
/**
 * Retrieve the length field value from the given BD.  The returned value is
 * the same as what was written with XAxiDma_BdSetLength(). Note that in the
 * this value does not reflect the real length of received data.
 * See the comments of XAxiDma_BdSetLength() for more details. To obtain the
 * actual transfer length, use XAxiDma_BdGetActualLength().
 *
 * @param  BdPtr is the BD to operate on.
 *
 * @return
 * The length value set in the BD.
 *
 * @note
 * C-style signature:
 *    u32 XAxiDma_BdGetLength(XAxiDma_Bd* BdPtr)
 *
 *****************************************************************************/
#define XAxiDma_BdGetLength(BdPtr)                      \
	(XAxiDma_BdRead((BdPtr), XAXIDMA_BD_CTRL_LEN_OFFSET) & \
		XAXIDMA_BD_CTRL_LENGTH_MASK)


/*****************************************************************************/
/**
 * Set the ID field of the given BD. The ID is an arbitrary piece of data the
 * application can associate with a specific BD.
 *
 * @param  BdPtr is the BD to operate on
 * @param  Id is a 32 bit quantity to set in the BD
 *
 * @note
 * C-style signature:
 *    void XAxiDma_BdSetId(XAxiDma_Bd* BdPtr, void Id)
 *
 *****************************************************************************/
#define XAxiDma_BdSetId(BdPtr, Id)                                      \
	(XAxiDma_BdWrite((BdPtr), XAXIDMA_BD_ID_OFFSET, (u32)(Id)))


/*****************************************************************************/
/**
 * Retrieve the ID field of the given BD previously set with XAxiDma_BdSetId.
 *
 * @param  BdPtr is the BD to operate on
 *
 * @note
 * C-style signature:
 *    u32 XAxiDma_BdGetId(XAxiDma_Bd* BdPtr)
 *
 *****************************************************************************/
#define XAxiDma_BdGetId(BdPtr) (XAxiDma_BdRead((BdPtr), XAXIDMA_BD_ID_OFFSET))

/*****************************************************************************/
/**
 * Get the BD's buffer address
 *
 * @param  BdPtr is the BD to operate on
 *
 * @note
 * C-style signature:
 *    u32 XAxiDma_BdGetBufAddr(XAxiDma_Bd* BdPtr)
 *
 *****************************************************************************/
#define XAxiDma_BdGetBufAddr(BdPtr)                     \
	(XAxiDma_BdRead((BdPtr), XAXIDMA_BD_BUFA_OFFSET))

/*****************************************************************************/
/**
 * Check whether a BD has completed in hardware. This BD has been submitted
 * to hardware. The application can use this function to poll for the
 * completion of the BD.
 *
 * This function may not work if the BD is in cached memory.
 *
 * @param  BdPtr is the BD to check on
 *
 * @return
 * 0 if not complete, XAXIDMA_BD_STS_COMPLETE_MASK if completed, may contain
 * XAXIDMA_BD_STS_*_ERR_MASK bits.
 *
 * @note
 * C-style signature:
 *    int XAxiDma_BdHwCompleted(XAxiDma_Bd* BdPtr)
 *
 *****************************************************************************/
#define XAxiDma_BdHwCompleted(BdPtr)                     \
	(XAxiDma_BdRead((BdPtr), XAXIDMA_BD_STS_OFFSET) & \
		XAXIDMA_BD_STS_COMPLETE_MASK)

/*****************************************************************************/
/**
 * Get the actual transfer length of a BD. The BD has completed in hw.
 *
 * This function may not work if the BD is in cached memory.
 *
 * @param  BdPtr is the BD to check on
 *
 * @note
 * C-style signature:
 *    int XAxiDma_BdGetActualLength(XAxiDma_Bd* BdPtr)
 *
 *****************************************************************************/
#define XAxiDma_BdGetActualLength(BdPtr)                     \
	(XAxiDma_BdRead((BdPtr), XAXIDMA_BD_STS_OFFSET) & \
		XAXIDMA_BD_STS_ACTUAL_LEN_MASK)

/************************** Function Prototypes ******************************/

int XAxiDma_BdSetLength(XAxiDma_Bd* BdPtr, u32 LenBytes);
int XAxiDma_BdSetBufAddr(XAxiDma_Bd* BdPtr, u32 Addr);
int XAxiDma_BdSetAppWord(XAxiDma_Bd * BdPtr, int Offset, u32 Word);
u32 XAxiDma_BdGetAppWord(XAxiDma_Bd * BdPtr, int Offset, int *Valid);
void XAxiDma_BdSetCtrl(XAxiDma_Bd *BdPtr, u32 Data);
 
/* Debug utility
 */
void XAxiDma_DumpBd(XAxiDma_Bd* BdPtr);

#ifdef __cplusplus
}
#endif

#endif /* end of protection macro */
