/******************************************************************************
*
*     This program is free software; you can redistribute it and/or modify it
*     under the terms of the GNU General Public License as published by the
*     Free Software Foundation; either version 2 of the License, or (at your
*     option) any later version.
*
*     (c) Copyright 2008 Xilinx Inc.
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

/* polled mode LLTEMAC DMA driver to support u-boot */

/*
 * ENET_MAX_MTU and ENET_MAX_MTU_ALIGNED are set from
 * PKTSIZE and PKTSIZE_ALIGN (include/net.h)
 */

#define LLTEMAC_MAX_MTU_ALIGNED   	PKTSIZE_ALIGN
#define ENET_ADDR_LENGTH       		6

/* hardcoded MAC address for the Xilinx EMAC Core when env is nowhere*/
#ifdef CFG_ENV_IS_NOWHERE
static u8 EMACAddr[ENET_ADDR_LENGTH] = { 0x00, 0x0a, 0x35, 0x01, 0x02, 0x03 };
#endif

static int initialized = 0;

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */

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
static u8 gmii_addr;		/* The GMII address of the PHY */


/* function prototypes */

static int detect_phy();
static void set_mac_speed();
static void phy_setup();

/* Detect the PHY address by scanning addresses 0 to 31 and
 * looking at the MII status register (register 1) and assuming
 * the PHY supports 10Mbps full/half duplex. Feel free to change
 * this code to match your PHY, or hardcode the address if needed.
 */
/* Use MII register 1 (MII status register) to detect PHY */
#define PHY_DETECT_REG  1

/* Mask used to verify certain PHY features (or register contents)
 * in the register above:
 *  0x1000: 10Mbps full duplex support
 *  0x0800: 10Mbps half duplex support
 *  0x0008: Auto-negotiation support
 */
#define PHY_DETECT_MASK 0x1808

static int detect_phy()
{
	u16 phy_reg;
	u32 phy_addr;

	for (phy_addr = 31; phy_addr > 0; phy_addr--) {
		XLlTemac_PhyRead(&LlTemac, phy_addr, PHY_DETECT_REG, &phy_reg);

		if ((phy_reg != 0xFFFF) &&
		    ((phy_reg & PHY_DETECT_MASK) == PHY_DETECT_MASK)) {
			/* Found a valid PHY address */
			debug_printf("XTemac: PHY detected at address %d.\n", phy_addr);
			return phy_addr;
		}
	}

	debug_printf("XTemac: No PHY detected.  Assuming a PHY at address 0\n");
	return 0;		/* default to zero */
}

#define CONFIG_XILINX_LLTEMAC_MARVELL_88E1111_GMII

/*
 * This function sets up MAC's speed according to link speed of PHY
 */
static void set_mac_speed()
{
	u16 phylinkspeed;

#ifdef CONFIG_XILINX_LLTEMAC_MARVELL_88E1111_GMII
	/*
	 * This function is specific to MARVELL 88E1111 PHY chip on
	 * many Xilinx boards and assumes GMII interface is being used
	 * by the TEMAC.
	 */

#define MARVELL_88E1111_PHY_SPECIFIC_STATUS_REG_OFFSET  17
#define MARVELL_88E1111_LINKSPEED_MARK                  0xC000
#define MARVELL_88E1111_LINKSPEED_SHIFT                 14
#define MARVELL_88E1111_LINKSPEED_1000M                 0x0002
#define MARVELL_88E1111_LINKSPEED_100M                  0x0001
#define MARVELL_88E1111_LINKSPEED_10M                   0x0000
	u16 RegValue;

	XLlTemac_PhyRead(&LlTemac, gmii_addr,
	 		MARVELL_88E1111_PHY_SPECIFIC_STATUS_REG_OFFSET,
			&RegValue);
	/* Get current link speed */
	phylinkspeed = (RegValue & MARVELL_88E1111_LINKSPEED_MARK)
		>> MARVELL_88E1111_LINKSPEED_SHIFT;

	/* Update TEMAC speed accordingly */
	switch (phylinkspeed) {
	case (MARVELL_88E1111_LINKSPEED_1000M):
		XLlTemac_SetOperatingSpeed(&LlTemac, 1000);
		debug_printf("XLlTemac: speed set to 1000Mb/s\n");
		break;
	case (MARVELL_88E1111_LINKSPEED_100M):
		XLlTemac_SetOperatingSpeed(&LlTemac, 100);
		debug_printf("XLlTemac: speed set to 100Mb/s\n");
		break;
	case (MARVELL_88E1111_LINKSPEED_10M):
		XLlTemac_SetOperatingSpeed(&LlTemac, 10);
		debug_printf("XLlTemac: speed set to 10Mb/s\n");
		break;
	default:
		XLlTemac_SetOperatingSpeed(&LlTemac, 1000);
		debug_printf("XLlTemac: speed defaults to 1000Mb/s\n");
		break;
	}
#else
	/* add your PHY specific code here for other PHYs*/
#endif

}

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
	LlTemacConfig.BaseAddress = XPAR_LLTEMAC_0_BASEADDR;
	LlTemacConfig.TxCsum = 0;
	LlTemacConfig.RxCsum = 0;
	LlTemacConfig.PhyType = 0;
	LlTemacConfig.TemacIntr = 0;
	LlTemacConfig.LLDevType = XPAR_LL_DMA;
	LlTemacConfig.LLDevBaseAddress = XPAR_LLTEMAC_0_LLINK_CONNECTED_BASEADDR;

	Status = XLlTemac_CfgInitialize(&LlTemac, &LlTemacConfig, XPAR_LLTEMAC_0_BASEADDR); 
	if (Status != XST_SUCCESS) {
		debug_printf("Error in initialize\r\n");
		return XST_FAILURE;
	}

	/* now we can reset the device */
	XLlTemac_Reset(&LlTemac, XTE_RESET_HARD);

	/* Reset on TEMAC also resets PHY. Give it some time to finish negotiation
	 * before we move on */
	udelay(2 * 1000000);

	debug_printf("Initializing DMA...\r\n");

	XLlDma_Initialize(&LlDma, LlTemac.Config.LLDevBaseAddress);

	/*
	 * Check whether the IPIF interface is correct for this example
	 */
	dma_mode = XLlTemac_IsDma(&LlTemac);
	if (!dma_mode) {
		debug_printf("Device HW not configured for SGDMA mode\r\n");
        	return 1;
	}

#ifdef CFG_ENV_IS_NOWHERE
	memcpy(bis->bi_enetaddr, EMACAddr, 6);
#endif

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

	/*
	 * Create the TxBD ring
	 */
	Status = XLlDma_BdRingCreate(TxRingPtr, (u32) &TxBdSpace,
					(u32) &TxBdSpace, BD_ALIGNMENT, TXBD_CNT);
	if (Status != XST_SUCCESS) {
		debug_printf("Error setting up TxBD space");
		return 1;
	}
	/*
	 * We reuse the bd template, as the same one will work for both rx and tx.
	 */	Status = XLlDma_BdRingClone(TxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {
		debug_printf("Error initializing TxBD space");
		return 1;
	}

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
	gmii_addr = detect_phy();
	set_mac_speed();
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

int
eth_send(volatile void *ptr, int len)
{
	int Status;
	XLlDma_Bd *TxBdPtr;
	XLlDma_BdRing *TxRingPtr = &XLlDma_mGetTxRing(&LlDma);

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
