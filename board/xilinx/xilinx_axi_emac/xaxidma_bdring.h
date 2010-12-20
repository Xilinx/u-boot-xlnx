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
* @file xaxidma_bdring.h
*
* This file contains DMA channel related structure and constant definition
* as well as function prototypes. Each DMA channel is managed by a Buffer
* Descriptor ring, and XAxiDma_BdRing is chosen as the symbol prefix used in
* this file. See xaxidma.h for more information on how a BD ring is managed.
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
******************************************************************************/

#ifndef XAXIDMA_BDRING_H_    /* prevent circular inclusions */
#define XAXIDMA_BDRING_H_

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/

#include "xstatus.h"
#include "xaxidma_bd.h"

/************************** Constant Definitions *****************************/
/* State of a DMA channel
 */
#define AXIDMA_CHANNEL_NOT_HALTED	1
#define AXIDMA_CHANNEL_HALTED	2

/* Argument constant to simplify argument setting
 */
#define XAXIDMA_NO_CHANGE            0xFFFFFFFF
#define XAXIDMA_ALL_BDS              0x0FFFFFFF /* 268 Million */

/**************************** Type Definitions *******************************/

/** Container structure for descriptor storage control. If address translation
 * is enabled, then all addresses and pointers excluding FirstBdPhysAddr are
 * expressed in terms of the virtual address.
 */
typedef struct {
	u32 ChanBase;			/**< physical base address*/

	int IsRxChannel;	/**< Is this a receive channel */
	volatile int RunState;		/**< Whether channel is running */ 
	int HasStsCntrlStrm; /**< Whether has stscntrl stream */
	int HasDRE;
	int DataWidth;

	u32 FirstBdPhysAddr;   /**< Physical address of 1st BD in list */
	u32 FirstBdAddr;       /**< Virtual address of 1st BD in list */
	u32 LastBdAddr;	       /**< Virtual address of last BD in the list */
	u32 Length;	       /**< Total size of ring in bytes */
	u32 Separation;	       /**< Number of bytes between the starting
	                            address of adjacent BDs */
	XAxiDma_Bd *FreeHead;   /**< First BD in the free group */
	XAxiDma_Bd *PreHead;    /**< First BD in the pre-work group */
	XAxiDma_Bd *HwHead;     /**< First BD in the work group */
	XAxiDma_Bd *HwTail;     /**< Last BD in the work group */
	XAxiDma_Bd *PostHead;   /**< First BD in the post-work group */
	XAxiDma_Bd *BdaRestart; /**< BD to load when channel is started */
	int FreeCnt;	       /**< Number of allocatable BDs in free group */
	int PreCnt;	       /**< Number of BDs in pre-work group */
	int HwCnt;	       /**< Number of BDs in work group */
	int PostCnt;	       /**< Number of BDs in post-work group */
	int AllCnt;	       /**< Total Number of BDs for channel */
} XAxiDma_BdRing;

/***************** Macros (Inline Functions) Definitions *********************/

/*****************************************************************************/
/**
* Use this macro at initialization time to determine how many BDs will fit
* within the given memory constraints.
*
* The results of this macro can be provided to XAxiDma_BdRingCreate().
*
* @param Alignment specifies what byte alignment the BDs must fall on and
*        must be a power of 2 to get an accurate calculation (32, 64, 126,...)
* @param Bytes is the number of bytes to be used to store BDs.
*
* @return Number of BDs that can fit in the given memory area
*
* @note
* C-style signature:
*    int XAxiDma_BdRingCntCalc(u32 Alignment, u32 Bytes)
*
******************************************************************************/
#define XAxiDma_BdRingCntCalc(Alignment, Bytes)                           \
	(int)((Bytes)/((sizeof(XAxiDma_Bd)+((Alignment)-1))&~((Alignment)-1)))

/*****************************************************************************/
/**
* Use this macro at initialization time to determine how many bytes of memory
* are required to contain a given number of BDs at a given alignment.
*
* @param Alignment specifies what byte alignment the BDs must fall on. This
*        parameter must be a power of 2 to get an accurate calculation (32, 64,
*        128,...)
* @param NumBd is the number of BDs to calculate memory size requirements for
*
* @return The number of bytes of memory required to create a BD list with the
*         given memory constraints.
*
* @note
* C-style signature:
*    int XAxiDma_BdRingMemCalc(u32 Alignment, u32 NumBd)
*
******************************************************************************/
#define XAxiDma_BdRingMemCalc(Alignment, NumBd)			\
	(int)((sizeof(XAxiDma_Bd)+((Alignment)-1)) & ~((Alignment)-1))*(NumBd)

/****************************************************************************/
/**
* Return the total number of BDs allocated by this channel with
* XAxiDma_BdRingCreate().
*
* @param  RingPtr is the BD ring to operate on.
*
* @return The total number of BDs allocated for this channel.
*
* @note
* C-style signature:
*    int XAxiDma_BdRingGetCnt(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingGetCnt(RingPtr) ((RingPtr)->AllCnt)

/****************************************************************************/
/**
* Return the number of BDs allocatable with XAxiDma_BdRingAlloc() for pre-
* processing.
*
* @param  RingPtr is the BD ring to operate on.
*
* @return The number of BDs currently allocatable.
*
* @note
* C-style signature:
*    int XAxiDma_BdRingGetFreeCnt(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingGetFreeCnt(RingPtr)  ((RingPtr)->FreeCnt)


/****************************************************************************/
/**
* Snap shot the latest BD a BD ring is processing.
*
* @param  RingPtr is the BD ring to operate on.
*
* @return None
*
* @note
* C-style signature:
*    void XAxiDma_BdRingSnapShotCurrBd(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingSnapShotCurrBd(RingPtr)				  \
	{								  \
		(RingPtr)->BdaRestart = 				  \
			(XAxiDma_Bd *)XAxiDma_ReadReg((RingPtr)->ChanBase, \
					XAXIDMA_CDESC_OFFSET);		  \
	}

/****************************************************************************/
/**
* Get the BD a BD ring is processing.
*
* @param  RingPtr is the BD ring to operate on.
*
* @return
*   The current BD that the BD ring is working on
*
* @note
* C-style signature:
*    XAxiDma_Bd * XAxiDma_BdRingGetCurrBd(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingGetCurrBd(RingPtr)               \
	(XAxiDma_Bd *)XAxiDma_ReadReg((RingPtr)->ChanBase, \
					XAXIDMA_CDESC_OFFSET)              \

/****************************************************************************/
/**
* Return the next BD in the ring.
*
* @param  RingPtr is the BD ring to operate on.
* @param  BdPtr is the current BD.
*
* @return The next BD in the ring relative to the BdPtr parameter.
*
* @note
* C-style signature:
*    XAxiDma_Bd *XAxiDma_BdRingNext(XAxiDma_BdRing* RingPtr, XAxiDma_Bd *BdPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingNext(RingPtr, BdPtr)			\
		(((u32)(BdPtr) >= (RingPtr)->LastBdAddr) ?	\
			(XAxiDma_Bd*)(RingPtr)->FirstBdAddr :	\
			(XAxiDma_Bd*)((u32)(BdPtr) + (RingPtr)->Separation))

/****************************************************************************/
/**
* Return the previous BD in the ring.
*
* @param  RingPtr is the DMA channel to operate on.
* @param  BdPtr is the current BD.
*
* @return The previous BD in the ring relative to the BdPtr parameter.
*
* @note
* C-style signature:
*    XAxiDma_Bd *XAxiDma_BdRingPrev(XAxiDma_BdRing* RingPtr, XAxiDma_Bd *BdPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingPrev(RingPtr, BdPtr)				\
		(((u32)(BdPtr) <= (RingPtr)->FirstBdAddr) ?		\
			(XAxiDma_Bd*)(RingPtr)->LastBdAddr :		\
			(XAxiDma_Bd*)((u32)(BdPtr) - (RingPtr)->Separation))

/****************************************************************************/
/**
* Retrieve the contents of the channel status register
*
* @param  RingPtr is the channel instance to operate on.
*
* @return Current contents of status register 
*
* @note
* C-style signature:
*    u32 XAxiDma_BdRingGetSr(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingGetSr(RingPtr)				\
		XAxiDma_ReadReg((RingPtr)->ChanBase, XAXIDMA_SR_OFFSET)

/****************************************************************************/
/**
* Get error bits of a DMA channel
*
* @param  RingPtr is the channel instance to operate on.
*
* @return 
*  Rrror bits in the status register, they should be interpreted with
*  XAXIDMA_ERR_*_MASK defined in xaxidma_hw.h	
*
* @note
* C-style signature:
*    u32 XAxiDma_BdRingGetError(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingGetError(RingPtr)				\
		(XAxiDma_ReadReg((RingPtr)->ChanBase, XAXIDMA_SR_OFFSET) \
			& XAXIDMA_ERR_ALL_MASK)

/****************************************************************************/
/**
* Check whether a DMA channel is started, meaning the channel is not halted.
*
* @param  RingPtr is the channel instance to operate on.
*
* @return
*  - 1 if channel is started
*  - 0 otherwise
*
* @note
* C-style signature:
*    int XAxiDma_BdRingHwIsStarted(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingHwIsStarted(RingPtr)				\
		((XAxiDma_ReadReg((RingPtr)->ChanBase, XAXIDMA_SR_OFFSET) \
			& XAXIDMA_HALTED_MASK) ? FALSE : TRUE)

/****************************************************************************/
/**
* Check if the current DMA channel is busy with a DMA operation.
*
* @param  RingPtr is the channel instance to operate on.
*
* @return
*  - 1 if the DMA is busy.
*  - 0 otherwise
*
* @note
* C-style signature:
*    int XAxiDma_BdRingBusy(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingBusy(RingPtr)					 \
		(XAxiDma_BdRingHwIsStarted(RingPtr) &&		\
		((XAxiDma_ReadReg((RingPtr)->ChanBase, XAXIDMA_SR_OFFSET) \
			& XAXIDMA_IDLE_MASK) ? FALSE : TRUE))

/****************************************************************************/
/**
* Set interrupt enable bits for a channel. This operation will modify the
* XAXIDMA_CR_OFFSET register.
*
* @param  RingPtr is the channel instance to operate on.
* @param  Mask consists of the interrupt signals to enable.
*         Bits not specified in the mask are not affected.
*
* @note
* C-style signature:
*    void XAxiDma_BdRingIntEnable(XAxiDma_BdRing* RingPtr, u32 Mask)
*
*****************************************************************************/
#define XAxiDma_BdRingIntEnable(RingPtr, Mask)			\
		(XAxiDma_WriteReg((RingPtr)->ChanBase, XAXIDMA_CR_OFFSET, \
			XAxiDma_ReadReg((RingPtr)->ChanBase, XAXIDMA_CR_OFFSET) \
			| ((Mask) & XAXIDMA_IRQ_ALL_MASK)))


/****************************************************************************/
/**
* Get enabled interrupts of a channel. It is in XAXIDMA_CR_OFFSET register.
*
* @param  RingPtr is the channel instance to operate on.
* @return Enabled interrupts of a channel. Use XAXIDMA_IRQ_* defined in
*         xaxidma_hw.h to interpret this returned value.
*
* @note
* C-style signature:
*    u32 XAxiDma_BdRingIntGetEnabled(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingIntGetEnabled(RingPtr)				\
	(XAxiDma_ReadReg((RingPtr)->ChanBase, XAXIDMA_CR_OFFSET) \
		& XAXIDMA_IRQ_ALL_MASK)

/****************************************************************************/
/**
* Clear interrupt enable bits for a channel. It modifies the
* XAXIDMA_CR_OFFSET register.
*
* @param  RingPtr is the channel instance to operate on.
* @param  Mask consists of the interrupt signals to disable.
*         Bits not specified in the Mask are not affected.
*
* @note
* C-style signature:
*    void XAxiDma_BdRingIntDisable(XAxiDma_BdRing* RingPtr, u32 Mask)
*
*****************************************************************************/
#define XAxiDma_BdRingIntDisable(RingPtr, Mask)				\
		(XAxiDma_WriteReg((RingPtr)->ChanBase, XAXIDMA_CR_OFFSET, \
		XAxiDma_ReadReg((RingPtr)->ChanBase, XAXIDMA_CR_OFFSET) & \
			~((Mask) & XAXIDMA_IRQ_ALL_MASK)))

/****************************************************************************/
/**
* Retrieve the contents of the channel's IRQ register XAXIDMA_SR_OFFSET. This
* operation can be used to see which interrupts are pending.
*
* @param  RingPtr is the channel instance to operate on.
*
* @return Current contents of the IRQ_OFFSET register. Use XAXIDMA_IRQ_***
*         values defined in xaxidma_hw.h to interpret the returned value.
*
* @note
* C-style signature:
*    u32 XAxiDma_BdRingGetIrq(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingGetIrq(RingPtr)				\
		(XAxiDma_ReadReg((RingPtr)->ChanBase, XAXIDMA_SR_OFFSET) \
			& XAXIDMA_IRQ_ALL_MASK)

/****************************************************************************/
/**
* Acknowledge asserted interrupts. It modifies XAXIDMA_SR_OFFSET register.
* A mask bit set for an unasserted interrupt has no effect.
*
* @param  RingPtr is the channel instance to operate on.
* @param  Mask are the interrupt signals to acknowledge
*
* @note
* C-style signature:
*    void XAxiDma_BdRingAckIrq(XAxiDma_BdRing* RingPtr)
*
*****************************************************************************/
#define XAxiDma_BdRingAckIrq(RingPtr, Mask)				\
		XAxiDma_WriteReg((RingPtr)->ChanBase, XAXIDMA_SR_OFFSET,\
			(Mask) & XAXIDMA_IRQ_ALL_MASK)

/************************* Function Prototypes ******************************/

/*
 * Descriptor ring functions xaxidma_bdring.c
 */
int XAxiDma_StartBdRingHw(XAxiDma_BdRing* RingPtr);
int XAxiDma_BdRingCreate(XAxiDma_BdRing * RingPtr, u32 PhysAddr,
		u32 VirtAddr, u32 Alignment, int BdCount);
int XAxiDma_BdRingClone(XAxiDma_BdRing * RingPtr, XAxiDma_Bd * SrcBdPtr);
int XAxiDma_BdRingAlloc(XAxiDma_BdRing * RingPtr, int NumBd,
		XAxiDma_Bd ** BdSetPtr);
int XAxiDma_BdRingUnAlloc(XAxiDma_BdRing * RingPtr, int NumBd,
		XAxiDma_Bd * BdSetPtr);
int XAxiDma_BdRingToHw(XAxiDma_BdRing * RingPtr, int NumBd,
		XAxiDma_Bd * BdSetPtr);
int XAxiDma_BdRingFromHw(XAxiDma_BdRing * RingPtr, int BdLimit,
		XAxiDma_Bd ** BdSetPtr);
int XAxiDma_BdRingFree(XAxiDma_BdRing * RingPtr, int NumBd,
		XAxiDma_Bd * BdSetPtr);
int XAxiDma_BdRingStart(XAxiDma_BdRing * RingPtr);
int XAxiDma_BdRingSetCoalesce(XAxiDma_BdRing * RingPtr, u32 Counter, u32 Timer);
void XAxiDma_BdRingGetCoalesce(XAxiDma_BdRing * RingPtr,
		u32 *CounterPtr, u32 *TimerPtr);

/* The following functions are for debug only
 */
int XAxiDma_BdRingCheck(XAxiDma_BdRing * RingPtr);
void XAxiDma_BdRingDumpRegs(XAxiDma_BdRing *RingPtr);
#ifdef __cplusplus
}
#endif

#endif /* end of protection macro */

