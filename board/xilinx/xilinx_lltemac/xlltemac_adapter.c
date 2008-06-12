/******************************************************************************
*
*     Author: Xilinx, Inc.
*
*
*     This program is free software; you can redistribute it and/or modify it
*     under the terms of the GNU General Public License as published by the
*     Free Software Foundation; either version 2 of the License, or (at your
*     option) any later version.
*
*
*     XILINX IS PROVIDING THIS DESIGN, CODE, OR INFORMATION "AS IS" AS A
*     COURTESY TO YOU. BY PROVIDING THIS DESIGN, CODE, OR INFORMATION AS
*     ONE POSSIBLE IMPLEMENTATION OF THIS FEATURE, APPLICATION OR STANDARD,
*     XILINX IS MAKING NO REPRESENTATION THAT THIS IMPLEMENTATION IS FREE
*     FROM ANY CLAIMS OF INFRINGEMENT, AND YOU ARE RESPONSIBLE FOR OBTAINING
*     ANY THIRD PARTY RIGHTS YOU MAY REQUIRE FOR YOUR IMPLEMENTATION.
*     XILINX EXPRESSLY DISCLAIMS ANY WARRANTY WHATSOEVER WITH RESPECT TO
*     THE ADEQUACY OF THE IMPLEMENTATION, INCLUDING BUT NOT LIMITED TO ANY
*     WARRANTIES OR REPRESENTATIONS THAT THIS IMPLEMENTATION IS FREE FROM
*     CLAIMS OF INFRINGEMENT, IMPLIED WARRANTIES OF MERCHANTABILITY AND
*     FITNESS FOR A PARTICULAR PURPOSE.
*
*
*     Xilinx hardware products are not intended for use in life support
*     appliances, devices, or systems. Use in such applications is
*     expressly prohibited.
*
*
*     (c) Copyright 2002-2004 Xilinx Inc.
*     All rights reserved.
*
*
*     You should have received a copy of the GNU General Public License along
*     with this program; if not, write to the Free Software Foundation, Inc.,
*     675 Mass Ave, Cambridge, MA 02139, USA.
*
******************************************************************************/

#include <config.h>
#include <common.h>
#include <net.h>
#include "xlltemac.h"
#include "xlldma.h"
#include "xparameters.h"

//#if defined(XPAR_LLTEMAC_0_DEVICE_ID)
/*
 * ENET_MAX_MTU and ENET_MAX_MTU_ALIGNED are set from
 * PKTSIZE and PKTSIZE_ALIGN (include/net.h)
 */

#define LLTEMAC_MAX_MTU_ALIGNED   PKTSIZE_ALIGN
#define ENET_ADDR_LENGTH       6

/* hardcoded MAC address for the Xilinx EMAC Core when env is nowhere*/
//#ifdef CFG_ENV_IS_NOWHERE
static u8 EMACAddr[ENET_ADDR_LENGTH] = { 0x00, 0x0a, 0x35, 0x01, 0x02, 0x03 };
//#endif

static int initialized = 0;

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the Tneeded parameters in one place.
 */
#define LLTEMAC_DEVICE_ID   XPAR_LLTEMAC_0_DEVICE_ID

#define RXBD_CNT        1 /* Number of RxBDs to use */
#define TXBD_CNT        1 /* Number of TxBDs to use */
#define BD_ALIGNMENT    XLLDMA_BD_MINIMUM_ALIGNMENT /* Byte alignment of BDs */

/*
 * Number of bytes to reserve for BD space for the number of BDs desired
 */
#define RXBD_SPACE_BYTES XLlDma_mBdRingMemCalc(BD_ALIGNMENT, RXBD_CNT)
#define TXBD_SPACE_BYTES XLlDma_mBdRingMemCalc(BD_ALIGNMENT, TXBD_CNT)

#ifdef DEBUG_LLTEMAC
#define debug_printf(x...) 	printf(x)
#else
#define debug_printf(x...) 
#endif

/*************************** Variable Definitions ****************************/

/*
 * Aligned memory segments to be used for buffer descriptors
 */
static char RxBdSpace[RXBD_SPACE_BYTES] __attribute__ ((aligned(BD_ALIGNMENT)));
static char TxBdSpace[TXBD_SPACE_BYTES] __attribute__ ((aligned(BD_ALIGNMENT)));

static char RxFrame[LLTEMAC_MAX_MTU_ALIGNED] __attribute__ ((aligned(8)));

static XLlTemac LlTemac;
static XLlDma LlDma;

void
eth_halt(void)
{
	if (initialized)
		(void) XLlTemac_Stop(&LlTemac);
}



int
eth_init(bd_t * bis)
{
	debug_printf("LLTEMAC Initialization Started\n\r");

	int Status;
	u32 Rdy;
	int dma_mode;
	XLlDma_BdRing *RxRingPtr = &XLlDma_mGetRxRing(&LlDma);
	XLlDma_BdRing *TxRingPtr = &XLlDma_mGetTxRing(&LlDma);
	XLlDma_Bd BdTemplate;
	XLlDma_Bd *RxBdPtr;
	XLlTemac_Config LlTemacConfig;

	/*************************************/
	/* Setup device for first-time usage */
	/*************************************/

	/*
 	 *  Initialize instance. Should be configured for SGDMA
	 */
	LlTemacConfig.BaseAddress = XPAR_XLLTEMAC_0_BASEADDR;
	LlTemacConfig.TxCsum = 0;
	LlTemacConfig.RxCsum = 0;
	LlTemacConfig.PhyType = 0;
	LlTemacConfig.TemacIntr = 0;
	LlTemacConfig.LLDevType = XPAR_LL_DMA;
	LlTemacConfig.LLDevBaseAddress = XPAR_XLLTEMAC_0_LLINK_CONNECTED_BASEADDR;

    Status = XLlTemac_CfgInitialize(&LlTemac, &LlTemacConfig, XPAR_XLLTEMAC_0_BASEADDR); 
    if (Status != XST_SUCCESS) {
        debug_printf("Error in initialize\r\n");
        return XST_FAILURE;
    }

	debug_printf("Initializing DMA...\r\n");

    XLlDma_Initialize(&LlDma, LlTemac.Config.LLDevBaseAddress);

    /*
     * Check whether the IPIF interface is correct for this example
     */
    dma_mode = XLlTemac_IsDma(&LlTemac);
    if (! dma_mode) {
        debug_printf
            ("Device HW not configured for SGDMA mode\r\n");
        return 1;
    }

//#ifdef CFG_ENV_IS_NOWHERE
	memcpy(bis->bi_enetaddr, EMACAddr, 6);
//#endif

	debug_printf("Setting the MAC address...\r\n");

    /*
     * Set the MAC address
     */
    Status = XLlTemac_SetMacAddress(&LlTemac, bis->bi_enetaddr);
    if (Status != XST_SUCCESS) {
        debug_printf("Error setting MAC address");
        return 1;
    }

    /*
     * Setup RxBD space.
     *
     * We have already defined a properly aligned area of memory to store RxBDs
     * at the beginning of this source code file so just pass its address into
     * the function. No MMU is being used so the physical and virtual addresses
     * are the same.
     *
     * Setup a BD template for the Rx channel. This template will be copied to
     * every RxBD. We will not have to explicitly set these again.
     */
    XLlDma_mBdClear(&BdTemplate);

    /*
     * Create the RxBD ring
     */
    Status = XLlDma_BdRingCreate(RxRingPtr, (u32) &RxBdSpace,
                     (u32) &RxBdSpace, BD_ALIGNMENT, RXBD_CNT);
    if (Status != XST_SUCCESS) {
        debug_printf("Error setting up RxBD space");
        return 1;
    }

//#ifdef NOT_SIMPLE
    Status = XLlDma_BdRingClone(RxRingPtr, &BdTemplate);
    if (Status != XST_SUCCESS) {
        debug_printf("Error initializing RxBD space");
        return 1;
    }
//#endif

    /*
     * Setup TxBD space.
     *
     * Like RxBD space, we have already defined a properly aligned area of
     * memory to use.
     */

    /*
     * Create the TxBD ring
     */
    Status = XLlDma_BdRingCreate(TxRingPtr, (u32) &TxBdSpace,
                     (u32) &TxBdSpace, BD_ALIGNMENT, TXBD_CNT);
    if (Status != XST_SUCCESS) {
        debug_printf("Error setting up TxBD space");
        return 1;
    }
//#ifdef NOT_SIMPLE
    /*
     * We reuse the bd template, as the same one will work for both rx and tx.
     */
    Status = XLlDma_BdRingClone(TxRingPtr, &BdTemplate);
    if (Status != XST_SUCCESS) {
        debug_printf("Error initializing TxBD space");
        return 1;
    }
//#endif

    /* Make sure the hard temac is ready */
    Rdy = XLlTemac_ReadReg(LlTemac.Config.BaseAddress,
                   XTE_RDY_OFFSET);
    while ((Rdy & XTE_RDY_HARD_ACS_RDY_MASK) == 0) {
        Rdy = XLlTemac_ReadReg(LlTemac.Config.BaseAddress,
                       XTE_RDY_OFFSET);
    }

    /*
     * Set PHY<-->MAC data clock
     */
    XLlTemac_SetOperatingSpeed(&LlTemac, 100);

    /*
     * Setting the operating speed of the MAC needs a delay.  There
     * doesn't seem to be register to poll, so please consider this
     * during your application design.
     */
    udelay(2 * 1000000);

    /*
     * Start the LLTEMAC device without any interrupts as it's polled
     */
    XLlTemac_Start(&LlTemac);

    /*
     * Allocate 1 RxBD. Note that TEMAC utilizes an in-place allocation
     * scheme. The returned Bd1Ptr will point to a free BD in the memory
     * segment setup with the call to XLlTemac_SgSetSpace()
     */
    Status = XLlDma_BdRingAlloc(RxRingPtr, 1, &RxBdPtr);
    if (Status != XST_SUCCESS) {
        debug_printf("Error allocating RxBD");
        return 1;
    }

    /*
     * Setup the BD. The BD template used in the call to XLlTemac_SgSetSpace()
     * set the "last" field of all RxBDs. Therefore we are not required to
     * issue a XLlDma_Bd_mSetLast(Bd1Ptr) here.
     */
    XLlDma_mBdSetBufAddr(RxBdPtr, &RxFrame);
    XLlDma_mBdSetLength(RxBdPtr, sizeof(RxFrame));
    XLlDma_mBdSetStsCtrl(RxBdPtr, XLLDMA_BD_STSCTRL_SOP_MASK | XLLDMA_BD_STSCTRL_EOP_MASK);

    /*
     * Enqueue to HW
     */
    Status = XLlDma_BdRingToHw(RxRingPtr, 1, RxBdPtr);
    if (Status != XST_SUCCESS) {
        debug_printf("Error committing RxBD to HW\n\r");
        return 1;
    }

    /*
     * Start DMA RX channel. Now it's ready to receive data.
     */
    Status = XLlDma_BdRingStart(RxRingPtr);
    if (Status != XST_SUCCESS) {
        debug_printf("Error starting DMA Rx\n\r"); 
        return 1;
    }

    /*
     * Start DMA TX channel. Transmission starts at once.
     */
    Status = XLlDma_BdRingStart(TxRingPtr);
    if (Status != XST_SUCCESS) {
        debug_printf("Error starting DMA Tx");
        return 1;
    }

	debug_printf("LLTEMAC Initialization complete\n\r");

	initialized = 1;

	return (0);
}

/*-----------------------------------------------------------------------------+
+-----------------------------------------------------------------------------*/
int
eth_send(volatile void *ptr, int len)
{
	int Status;
	XLlDma_Bd *TxBdPtr;
	XLlDma_BdRing *TxRingPtr = &XLlDma_mGetTxRing(&LlDma);

//	if (len > ENET_MAX_MTU)
//		len = ENET_MAX_MTU;

	/*
	 * Allocate, setup, and enqueue a TxBD. 
	 */
	Status = XLlDma_BdRingAlloc(TxRingPtr, 1, &TxBdPtr);
	if (Status != XST_SUCCESS) {
	        debug_printf("Error allocating TxBD\n\r");
	        return 0;
	}

	/*
	 * Setup TxBD
	 */
	XLlDma_mBdSetBufAddr(TxBdPtr, ptr);
	XLlDma_mBdSetLength(TxBdPtr, len);
	XLlDma_mBdSetStsCtrl(TxBdPtr, XLLDMA_BD_STSCTRL_SOP_MASK | XLLDMA_BD_STSCTRL_EOP_MASK);

	/*
	 * Enqueue to HW
	 */
	Status = XLlDma_BdRingToHw(TxRingPtr, 1, TxBdPtr);
	if (Status != XST_SUCCESS) {
		/*
		 * Undo BD allocation and exit
		 */
		XLlDma_BdRingUnAlloc(TxRingPtr, 1, TxBdPtr);
		debug_printf("Error committing TxBD to HW\n\r");
		return 0;
	}	

	debug_printf("Waiting for frame to be sent\n\r");

	/*
	 * Wait for transmission to complete
 	 */
	while (XLlDma_BdRingFromHw(TxRingPtr, XLLDMA_ALL_BDS, &TxBdPtr) == 0);

	debug_printf("Frame sent\n\r");

	/*
	 * Return the TxBD back to the channel for later allocation. Free the
	 * exact number we just post processed.
	 */
	Status = XLlDma_BdRingFree(TxRingPtr, 1, TxBdPtr);
	if (Status != XST_SUCCESS) {
		debug_printf("Error freeing up TxBDs\n\r");
		return 0;
	}

	return (1);
}

int
eth_rx(void)
{
	u32 RecvFrameLength;
	int Status;
	XLlDma_Bd *RxBdPtr;
	u32 BdCount;
	XLlDma_BdRing *RxRingPtr = &XLlDma_mGetRxRing(&LlDma);

	BdCount = XLlDma_BdRingFromHw(RxRingPtr, XLLDMA_ALL_BDS, &RxBdPtr);

	/* If there are any buffer descriptors ready to be received from the 
	 * DMA then send the buffer up the network stack
	 */
	if (BdCount > 0) {

		RecvFrameLength = XLlDma_mBdRead(RxBdPtr, XLLDMA_BD_USR4_OFFSET);

		debug_printf("Received a frame of %d bytes\r\n", RecvFrameLength);

		NetReceive((uchar *)RxFrame, RecvFrameLength);

		/*
		 * Return the RxBD back to the channel for later allocation. Free the
		 * exact number we just post processed.
		 */
		Status = XLlDma_BdRingFree(RxRingPtr, 1, RxBdPtr);
		if (Status != XST_SUCCESS) {
			debug_printf("Error freeing up RxBD\n\r");
			return 0;
		}

		/*
 		 * Allocate 1 RxBD. Note that TEMAC utilizes an in-place allocation
 		 * scheme. The returned Bd1Ptr will point to a free BD in the memory
		 * segment setup with the call to XLlTemac_SgSetSpace()	
		 */
		Status = XLlDma_BdRingAlloc(RxRingPtr, 1, &RxBdPtr);
		if (Status != XST_SUCCESS) {
			debug_printf("Error allocating RxBD");
			return 0;
		}
		
		/*
		 * Setup the BD. The BD template used in the call to XLlTemac_SgSetSpace()
		 * set the "last" field of all RxBDs. Therefore we are not required to
		 * issue a XLlDma_Bd_mSetLast(Bd1Ptr) here.
		 */
		XLlDma_mBdSetBufAddr(RxBdPtr, &RxFrame);
		XLlDma_mBdSetLength(RxBdPtr, sizeof(RxFrame));
		XLlDma_mBdSetStsCtrl(RxBdPtr, XLLDMA_BD_STSCTRL_SOP_MASK | 							XLLDMA_BD_STSCTRL_EOP_MASK);
		/*
		 * Enqueue to HW again so it can receive another frame	
		 */
		Status = XLlDma_BdRingToHw(RxRingPtr, 1, RxBdPtr);
		if (Status != XST_SUCCESS) {
		        debug_printf("Error committing RxBD to HW\n\r");
			return 0;
		}
		return 1;
	} else {
		return 0;
	}
}
