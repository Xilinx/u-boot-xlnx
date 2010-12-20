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
* @file xaxidma.c
*
* This file implements DMA engine-wise initialization and control functions. 
* For more information on the implementation of this driver, see xaxidma.h.
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
******************************************************************************/

/***************************** Include Files *********************************/

#include "xaxidma.h"
#include "malloc.h"

/************************** Constant Definitions *****************************/

/* Loop counter to check reset done
 */
#define XAXIDMA_RESET_TIMEOUT  500

/**************************** Type Definitions *******************************/


/***************** Macros (Inline Functions) Definitions *********************/


/************************** Function Prototypes ******************************/
static int XAxiDma_Start(XAxiDma * InstancePtr);
static int XAxiDma_Started(XAxiDma * InstancePtr);

#if (!defined(DEBUG)) 
extern int xil_printf(const char *format, ...);
#endif

/************************** Variable Definitions *****************************/

/*****************************************************************************/
/**
 * This function initializes a DMA engine.  This function must be called
 * prior to using a DMA engine. Initializing a engine includes setting
 * up the register base address, setting up the instance data, and ensuring the
 * hardware is in a quiescent state.
 *
 * @param  InstancePtr is a pointer to the DMA engine instance to be worked on.
 * @param  Config is a pointer to an XAxiDma_Config structure. It contains the
 *         information about the hardware build, including base address,
 *         and whether status control stream (StsCntrlStrm), MM2S and S2MM are
 *         included in the build. 
 *
 * @return 
 * - XST_SUCCESS for successful initialization
 * - XST_INVALID_PARAM if pointer to the configuration structure is NULL
 * - XST_DMA_ERROR if reset operation failed at the end of initialization 
 *
 * @note
 * We assume the hardware building tool will check and error out for a hardware
 * build that has no transfer channels.
 *****************************************************************************/
int XAxiDma_CfgInitialize(XAxiDma * InstancePtr, XAxiDma_Config *Config) {
	u32 BaseAddr;
	int TimeOut;
	
	InstancePtr->Initialized = 0;

	if(!Config) {
		return XST_INVALID_PARAM;
	}

	BaseAddr = Config->BaseAddr;

	/* Setup the instance */
	memset(InstancePtr, 0, sizeof(XAxiDma));
	InstancePtr->RegBase = BaseAddr;

	/* Get hardware setting information from the configuration structure 
	 */
	InstancePtr->HasMm2S = Config->HasMm2S; 
	InstancePtr->HasS2Mm = Config->HasS2Mm; 

	/* Initialize the ring structures */
	InstancePtr->TxBdRing.RunState = AXIDMA_CHANNEL_HALTED;
	InstancePtr->TxBdRing.IsRxChannel = 0;

	InstancePtr->RxBdRing.RunState = AXIDMA_CHANNEL_HALTED;
	InstancePtr->RxBdRing.IsRxChannel = 1;

	if (InstancePtr->HasMm2S) {
		InstancePtr->TxBdRing.ChanBase = BaseAddr + XAXIDMA_TX_OFFSET;

		InstancePtr->TxBdRing.HasStsCntrlStrm = Config->HasStsCntrlStrm;
		InstancePtr->TxBdRing.HasDRE = Config->HasMm2SDRE;
		InstancePtr->TxBdRing.DataWidth = 
		    ((unsigned int)Config->Mm2SDataWidth >> 3);
	}

	if (InstancePtr->HasS2Mm) {
		InstancePtr->RxBdRing.ChanBase = BaseAddr + XAXIDMA_RX_OFFSET;

		InstancePtr->RxBdRing.HasStsCntrlStrm = Config->HasStsCntrlStrm;
		InstancePtr->RxBdRing.HasDRE = Config->HasS2MmDRE;
		InstancePtr->RxBdRing.DataWidth = 
		    ((unsigned int)Config->S2MmDataWidth >> 3);
	}

	/* Reset the engine so the hardware starts from a known state
	 */
	XAxiDma_Reset(InstancePtr);

	/* At the initialization time, hardware should finish reset quickly
	 */ 
	TimeOut = XAXIDMA_RESET_TIMEOUT;

	while (TimeOut) {

		if(XAxiDma_ResetIsDone(InstancePtr)) {
			break;
		}

		TimeOut -= 1;

	}

	if (!TimeOut) {
		xdbg_printf(XDBG_DEBUG_ERROR, "Failed reset in initialize\n\r");

		/* Need system hard reset to recover
		 */
		InstancePtr->Initialized = 0;
		return XST_DMA_ERROR;
	}

	/* Initialization is successful
	 */
	InstancePtr->Initialized = 1;

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
* Reset both TX and RX channels of a DMA engine.
*
* Reset one channel resets the whole AXI DMA engine.
*
* Any DMA transaction in progress will finish gracefully before engine starts
* reset. Any other transactions that have been submitted to hardware will be
* discarded by the hardware.
*
* @param  InstancePtr is a pointer to the DMA engine instance to be worked on.
*
* @return
* None
*
* @note
*  After the reset: 
*	- All interrupts are disabled.
*	- Engine is halted
*
******************************************************************************/
void XAxiDma_Reset(XAxiDma * InstancePtr)
{
	u32 RegBase;
	XAxiDma_BdRing *TxRingPtr;
	XAxiDma_BdRing *RxRingPtr;

	TxRingPtr = XAxiDma_GetTxRing(InstancePtr);
	RxRingPtr = XAxiDma_GetRxRing(InstancePtr);

	/* Save the locations of current BDs both rings are working on
	 * before the reset so later we can resume the rings smoothly.
	 */
	XAxiDma_BdRingSnapShotCurrBd(TxRingPtr);
	XAxiDma_BdRingSnapShotCurrBd(RxRingPtr);

	/* Reset
	 */
	if (InstancePtr->HasMm2S) {
		RegBase = InstancePtr->RegBase + XAXIDMA_TX_OFFSET;
	}
	else {
		RegBase = InstancePtr->RegBase + XAXIDMA_RX_OFFSET;
	}

	XAxiDma_WriteReg(RegBase, XAXIDMA_CR_OFFSET, XAXIDMA_CR_RESET_MASK);

	/* Set TX/RX Channel state */
	if (InstancePtr->HasMm2S) {
		TxRingPtr->RunState = AXIDMA_CHANNEL_HALTED;
	}

	if (InstancePtr->HasS2Mm) {
		RxRingPtr->RunState = AXIDMA_CHANNEL_HALTED;
	}
}

/*****************************************************************************/
/**
*
* Check whether reset is done
*
* @param  InstancePtr is a pointer to the DMA engine instance to be worked on.
*
* @return
* 1 if reset is done, 0 otherwise
******************************************************************************/
int XAxiDma_ResetIsDone(XAxiDma * InstancePtr)
{
	u32 RegisterValue;
	XAxiDma_BdRing *TxRingPtr;
	XAxiDma_BdRing *RxRingPtr;

	TxRingPtr = XAxiDma_GetTxRing(InstancePtr);
	RxRingPtr = XAxiDma_GetRxRing(InstancePtr);

	/* Check transmit channel
	 */
	if (InstancePtr->HasMm2S) {
		RegisterValue = XAxiDma_ReadReg(TxRingPtr->ChanBase,
			XAXIDMA_CR_OFFSET);

		/* Reset is done when the reset bit is low
		 */
		if(RegisterValue & XAXIDMA_CR_RESET_MASK) {

			return 0;
		}
	}

	/* Check receive channel
	 */
	if (InstancePtr->HasS2Mm) {
		RegisterValue = XAxiDma_ReadReg(RxRingPtr->ChanBase,
			XAXIDMA_CR_OFFSET);

		/* Reset is done when the reset bit is low
		 */
		if(RegisterValue & XAXIDMA_CR_RESET_MASK) {

			return 0;
		}
	}

	return 1;
}
/*****************************************************************************/
/*
* Start the DMA engine.  
*
* Start a halted engine. Processing of BDs is not started.
*
* @param  InstancePtr is a pointer to the DMA engine instance to be worked on.
*
* @return
* - XST_SUCCESS for success
* - XST_NOT_SGDMA if the driver instance has not been initialized
* - XST_DMA_ERROR if starting the hardware channel fails
*
*****************************************************************************/
static int XAxiDma_Start(XAxiDma * InstancePtr)
{
	XAxiDma_BdRing *TxRingPtr;
	XAxiDma_BdRing *RxRingPtr;
	int Status;

	if (!InstancePtr->Initialized) {

		xdbg_printf(XDBG_DEBUG_ERROR, "Start: Driver not initialized %d\n\r",
			InstancePtr->Initialized);

		return XST_NOT_SGDMA;
	}
	
	if (InstancePtr->HasMm2S) {	
		TxRingPtr = XAxiDma_GetTxRing(InstancePtr);

		if (TxRingPtr->RunState == AXIDMA_CHANNEL_HALTED) {

			/* Start the channel
			 */
			Status = XAxiDma_StartBdRingHw(TxRingPtr);
			if (Status != XST_SUCCESS) {
				xdbg_printf(XDBG_DEBUG_ERROR,
				    "Start hw tx channel failed %d\n\r", Status);

				return XST_DMA_ERROR;
			}
 
			TxRingPtr->RunState = AXIDMA_CHANNEL_NOT_HALTED;
		}
	}

	if (InstancePtr->HasMm2S) {	
		RxRingPtr = XAxiDma_GetRxRing(InstancePtr);

		if (RxRingPtr->RunState != AXIDMA_CHANNEL_HALTED) {
			return XST_SUCCESS;
		}

		/* Start the channel
		 */
		Status = XAxiDma_StartBdRingHw(RxRingPtr);
		if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR,
			    "Start hw tx channel failed %d\n\r", Status);

			return XST_DMA_ERROR;
		}
 
		RxRingPtr->RunState = AXIDMA_CHANNEL_NOT_HALTED;
	}

	return XST_SUCCESS;
}
/*****************************************************************************/
/**
* Pause DMA transactions on both channels. 
*
* If the engine is running and doing transfers, this function does not stop
* the DMA transactions immediately, because then hardware will throw away
* our previously queued transfers. All submitted transfers will finish.
* Transfers submitted after this function will not start until
* XAxiDma_BdRingStart() or XAxiDma_Resume() is called.
*
* @param  InstancePtr is a pointer to the DMA engine instance to be worked on.
*
* @return
* - XST_SUCCESS if successful
* - XST_NOT_SGDMA, if the driver instance is not initialized
*
*****************************************************************************/
int XAxiDma_Pause(XAxiDma * InstancePtr)
{
	XAxiDma_BdRing *TxRingPtr;
	XAxiDma_BdRing *RxRingPtr;

	if (!InstancePtr->Initialized) {

		xdbg_printf(XDBG_DEBUG_ERROR, "Pause: Driver not initialized %d\n\r",
			InstancePtr->Initialized);

		return XST_NOT_SGDMA;
	}
	
	if (InstancePtr->HasMm2S) {	
		TxRingPtr = XAxiDma_GetTxRing(InstancePtr);

		/* If channel is halted, then we do not need to do anything
		 */ 
		TxRingPtr->RunState = AXIDMA_CHANNEL_HALTED;
	}

	if (InstancePtr->HasS2Mm) {	
		RxRingPtr = XAxiDma_GetRxRing(InstancePtr);

		/* If channel is halted, then we do not need to do anything
		 */ 
		RxRingPtr->RunState = AXIDMA_CHANNEL_HALTED;
	}

	return XST_SUCCESS;

}

/*****************************************************************************/
/**
* Resume DMA transactions on both channels. 
*
* @param  InstancePtr is a pointer to the DMA engine instance to be worked on.
*
* @return
* - XST_SUCCESS for success
* - XST_NOT_SGDMA if the driver instance has not been initialized
* - XST_DMA_ERROR if one of the channels fails to start
*
*****************************************************************************/
int XAxiDma_Resume(XAxiDma * InstancePtr)
{
	XAxiDma_BdRing *TxRingPtr;
	XAxiDma_BdRing *RxRingPtr;
	int Status;

	if (!InstancePtr->Initialized) {

		xdbg_printf(XDBG_DEBUG_ERROR, "Resume: Driver not initialized %d\n\r",
			InstancePtr->Initialized);

		return XST_NOT_SGDMA;
	}
	
	/* If the DMA engine is not running, start it. Start may fail.
	 */
	if (!XAxiDma_Started(InstancePtr)) {
		Status = XAxiDma_Start(InstancePtr);

		if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR, "Resume: failed to start "
				"engine %d\n\r", Status);

			return Status;
		}
	}

	/* Mark the state to be not halted
	 */
	if (InstancePtr->HasMm2S) {	
		TxRingPtr = XAxiDma_GetTxRing(InstancePtr);

		Status = XAxiDma_BdRingStart(TxRingPtr);
		if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR, "Resume: failed to start tx ring"
				"%d\n\r", Status);

			return XST_DMA_ERROR;
		}

		TxRingPtr->RunState = AXIDMA_CHANNEL_NOT_HALTED;
	}

	if (InstancePtr->HasS2Mm) {	
		RxRingPtr = XAxiDma_GetRxRing(InstancePtr);

		Status = XAxiDma_BdRingStart(RxRingPtr);
		if (Status != XST_SUCCESS) {
			xdbg_printf(XDBG_DEBUG_ERROR, "Resume: failed to start rx ring"
				"%d\n\r", Status);

			return XST_DMA_ERROR;
		}

		RxRingPtr->RunState = AXIDMA_CHANNEL_NOT_HALTED;
	}

	return XST_SUCCESS;
}

/*****************************************************************************/
/*
* Check whether the DMA engine is started.
*
* @param  InstancePtr is a pointer to the DMA engine instance to be worked on.
*
* @return
*  - 1 if engine is started
*  - 0 otherwise.
*****************************************************************************/
static int XAxiDma_Started(XAxiDma * InstancePtr)
{
	XAxiDma_BdRing *TxRingPtr;
	XAxiDma_BdRing *RxRingPtr;

	if (!InstancePtr->Initialized) {

		xdbg_printf(XDBG_DEBUG_ERROR, "Started: Driver not initialized %d\n\r",
			InstancePtr->Initialized);

		return 0;
	}
	
	if (InstancePtr->HasMm2S) {	
		TxRingPtr = XAxiDma_GetTxRing(InstancePtr);

		if (!XAxiDma_BdRingHwIsStarted(TxRingPtr)) {
			xdbg_printf(XDBG_DEBUG_ERROR,
				"Started: tx ring not started\n\r");

			return 0;
		}
	}

	if (InstancePtr->HasS2Mm) {	
		RxRingPtr = XAxiDma_GetRxRing(InstancePtr);

		if (!XAxiDma_BdRingHwIsStarted(RxRingPtr)) {
			xdbg_printf(XDBG_DEBUG_ERROR,
				"Started: rx ring not started\n\r");

			return 0;
		}
	}

	return 1;
}

