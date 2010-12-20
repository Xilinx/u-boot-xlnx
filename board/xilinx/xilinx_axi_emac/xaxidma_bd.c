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
 * @file xaxidma_bd.c
 *
 * Buffer descriptor (BD) management API implementation.
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

#include "xaxidma_bd.h"

#define xil_printf printf

/************************** Function Prototypes ******************************/
#if (!defined(DEBUG)) 
extern int xil_printf(const char *format, ...);
#endif

/*****************************************************************************/
/**
 * Set the length field for the given BD.
 *
 * Length has to be non-zero and less than XAXIDMA_MAX_TRANSFER_LEN.
 *
 * For TX channels, the value passed in should be the number of bytes to
 * transmit from the TX buffer associated with the given BD.
 *
 * For RX channels, the value passed in should be the size of the RX buffer
 * associated with the given BD in bytes. This is to notify the RX channel
 * the capability of the RX buffer to avoid buffer overflow. 
 *
 * The actual receive length can be equal or smaller than the specified length.
 * The actual transfer length will be updated by the hardware in the 
 * XAXIDMA_BD_STS_OFFSET word in the BD.
 *
 * @param  BdPtr is the BD to operate on.
 * @param  LenBytes is the requested transfer length 
 *
 * @returns
 * - XST_SUCCESS for success
 * - XST_INVALID_PARAM for invalid BD length
 *
 *****************************************************************************/
int XAxiDma_BdSetLength(XAxiDma_Bd *BdPtr, u32 LenBytes)
{
	if (LenBytes <= 0 || (LenBytes > XAXIDMA_MAX_TRANSFER_LEN)) {

		xdbg_printf(XDBG_DEBUG_ERROR, "invalid length %d\n",
		    (int)LenBytes);

		return XST_INVALID_PARAM;
	}

	XAxiDma_BdWrite((BdPtr), XAXIDMA_BD_CTRL_LEN_OFFSET, 
		((XAxiDma_BdRead((BdPtr), XAXIDMA_BD_CTRL_LEN_OFFSET) & \
		~XAXIDMA_BD_CTRL_LENGTH_MASK)) | LenBytes);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
 * Set the BD's buffer address.
 *
 * @param  BdPtr is the BD to operate on
 * @param  Addr is the address to set
 *
 * @return
 * - XST_SUCCESS if buffer address set successfully
 * - XST_INVALID_PARAM if hardware has no DRE and address is not aligned 
 *
 *****************************************************************************/
int XAxiDma_BdSetBufAddr(XAxiDma_Bd* BdPtr, u32 Addr) {
	u32 HasDRE;
	u8 WordLen;

	HasDRE = XAxiDma_BdRead(BdPtr, XAXIDMA_BD_HAS_DRE_OFFSET);
	WordLen = HasDRE & XAXIDMA_BD_WORDLEN_MASK;

	if (Addr & (WordLen - 1)) {
		if ((HasDRE & XAXIDMA_BD_HAS_DRE_MASK) == 0) {
			xil_printf("Error set buf addr %x with %x and %x, %x\n\r",
			    Addr, HasDRE, (WordLen - 1), Addr & (WordLen - 1));

			return XST_INVALID_PARAM;
		}
	}

	XAxiDma_BdWrite(BdPtr, XAXIDMA_BD_BUFA_OFFSET, Addr);

	return XST_SUCCESS;

}

/*****************************************************************************/
/**
 * Set the APP word at the specified APP word offset for a BD.
 *
 * @param BdPtr is the BD to operate on.
 * @param Offset is the offset inside the APP word, it is valid from 0 to 4
 * @param Word is the value to set
 *
 * @returns
 *  - XST_SUCCESS for success
 *  - XST_INVALID_PARAM under following error conditions:
 *  1) StsCntrlStrm is not built in hardware
 *  2) Offset is not in valid range
 *
 * @note
 *  If the hardware build has C_SG_USE_STSAPP_LENGTH set to 1, then the last 
 * APP word, XAXIDMA_LAST_APPWORD, must have non-zero value when AND with
 * 0x7FFFFF. Not doing so will cause the hardware to stall.
 *****************************************************************************/
int XAxiDma_BdSetAppWord(XAxiDma_Bd* BdPtr, int Offset, u32 Word) {

	if (XAxiDma_BdRead(BdPtr, XAXIDMA_BD_HAS_STSCNTRL_OFFSET) == 0) {

		xdbg_printf(XDBG_DEBUG_ERROR, "BdRingSetAppWord: no sts cntrl "
			"stream in hardware build, cannot set app word\n\r");

		return XST_INVALID_PARAM;
	}

	if ((Offset < 0) || (Offset > XAXIDMA_LAST_APPWORD)) {

		xdbg_printf(XDBG_DEBUG_ERROR, "BdRingSetAppWord: invalid offset %d",
			Offset);

		return XST_INVALID_PARAM;
	}

	XAxiDma_BdWrite(BdPtr, XAXIDMA_BD_USR0_OFFSET + Offset * 4, Word);

	return XST_SUCCESS;
}

/*****************************************************************************/
/**
 * Get the APP word at the specified APP word offset for a BD.
 *
 * @param  BdPtr is the BD to operate on.
 * @param  Offset is the offset inside the APP word, it is valid from 0 to 4
 * @param  Valid is to tell the caller whether parameters are valid 
 *
 * @returns 
 *  The APP word. Passed in parameter Valid holds 0 for failure, and 1 for
 *  success.
 *****************************************************************************/
u32 XAxiDma_BdGetAppWord(XAxiDma_Bd* BdPtr, int Offset, int *Valid) {

	*Valid = 0;

	if (XAxiDma_BdRead(BdPtr, XAXIDMA_BD_HAS_STSCNTRL_OFFSET) == 0) {

		xdbg_printf(XDBG_DEBUG_ERROR, "BdRingGetAppWord: no sts cntrl "
			"stream in hardware build, no app word available\n\r");

		return (u32)0;
	}

	if((Offset < 0) || (Offset > XAXIDMA_LAST_APPWORD)) {

		xdbg_printf(XDBG_DEBUG_ERROR, "BdRingGetAppWord: invalid offset %d",
			Offset);

		return (u32)0;
	}

	*Valid = 1;

	return XAxiDma_BdRead(BdPtr, XAXIDMA_BD_USR0_OFFSET + Offset * 4);
}


/*****************************************************************************/
/**
 * Set the control bits for a BD.
 *
 * @param BdPtr is the BD to operate on.
 * @param Data is the bit value to set
 *
 * @return
 * None
 *
 *****************************************************************************/
void XAxiDma_BdSetCtrl(XAxiDma_Bd* BdPtr, u32 Data) {
	u32 RegValue = XAxiDma_BdRead(BdPtr, XAXIDMA_BD_CTRL_LEN_OFFSET);

	RegValue &= ~XAXIDMA_BD_CTRL_ALL_MASK;

	RegValue |= (Data & XAXIDMA_BD_CTRL_ALL_MASK);

	XAxiDma_BdWrite((BdPtr), XAXIDMA_BD_CTRL_LEN_OFFSET, RegValue);

	return;
}

/*****************************************************************************/
/**
 * Dump the fields of a BD.
 *
 * @param BdPtr is the BD to operate on.
 *
 * @return
 * None
 *
 *****************************************************************************/
void XAxiDma_DumpBd(XAxiDma_Bd* BdPtr) {

	xil_printf("Dump BD %x:\n\r", (unsigned int)BdPtr);
	xil_printf("\tNext Bd Ptr: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_NDESC_OFFSET));
	xil_printf("\tBuff addr: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_BUFA_OFFSET));
	xil_printf("\tContrl len: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_CTRL_LEN_OFFSET));
	xil_printf("\tStatus: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_STS_OFFSET));
	xil_printf("\tAPP 0: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_USR0_OFFSET));
	xil_printf("\tAPP 1: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_USR1_OFFSET));
	xil_printf("\tAPP 2: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_USR2_OFFSET));
	xil_printf("\tAPP 3: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_USR3_OFFSET));
	xil_printf("\tAPP 4: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_USR4_OFFSET));
	xil_printf("\tSW ID: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_ID_OFFSET));
	xil_printf("\tStsCtrl: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr,
	           XAXIDMA_BD_HAS_STSCNTRL_OFFSET)); 
	xil_printf("\tDRE: %x\n\r",
	    (unsigned int)XAxiDma_BdRead(BdPtr, XAXIDMA_BD_HAS_DRE_OFFSET)); 

	xil_printf("\n\r");
}
