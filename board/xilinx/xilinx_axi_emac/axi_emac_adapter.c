/******************************************************************************
 *
 * (c) Copyright 2010 Xilinx, Inc. All rights reserved.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

// #define DEBUG 1

#include "xaxidma.h"
#include "xil_io.h"
#include <config.h>
#include <common.h>
#include <net.h>
#include <malloc.h>

/* some kind of wierd problem with debug infrastructure in the axiemac
   so turn off debug before it */

#undef DEBUG
#include "xaxiethernet.h"

/*************************** Constant Definitions ****************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define RXBD_CNT	1	/* Number of RxBDs to use */
#define TXBD_CNT	1	/* Number of TxBDs to use */
#define BD_ALIGNMENT	XAXIDMA_BD_MINIMUM_ALIGNMENT/* Byte alignment of BDs */

/*
 * Number of bytes to reserve for BD space for the number of BDs desired
 */
#define RXBD_SPACE_BYTES (XAxiDma_BdRingMemCalc(BD_ALIGNMENT, RXBD_CNT))
#define TXBD_SPACE_BYTES (XAxiDma_BdRingMemCalc(BD_ALIGNMENT, TXBD_CNT))

/*************************** Variable Definitions ****************************/

/* this driver only supports a single instance of the emac at this time */

XAxiEthernet AxiEmac;
XAxiDma AxiDma;
/*
 * Aligned memory segments to be used for buffer descriptors
 */
char RxBdSpace[RXBD_SPACE_BYTES] __attribute__ ((aligned(BD_ALIGNMENT)));
char TxBdSpace[TXBD_SPACE_BYTES] __attribute__ ((aligned(BD_ALIGNMENT)));

#define ENET_MAX_MTU PKTSIZE_ALIGN

static u8 RxFrame[ENET_MAX_MTU];

static int initialized = 0;

static XAxiDma_Config XAxiDma_ConfigTable[] =
{
	{
		XPAR_AXIDMA_0_DEVICE_ID,
		XPAR_AXIDMA_0_BASEADDR, 
		XPAR_AXIDMA_0_SG_INCLUDE_STSCNTRL_STRM,
		XPAR_AXIDMA_0_INCLUDE_MM2S,
		XPAR_AXIDMA_0_INCLUDE_MM2S_DRE,
#ifdef XPAR_AXIDMA_0_M_AXIS_MM2S_DATA_WIDTH
		XPAR_AXIDMA_0_M_AXIS_MM2S_DATA_WIDTH,
#else
		XPAR_AXI_DMA_ETHERNET_M_AXIS_MM2S_TDATA_WIDTH,
#endif
		XPAR_AXIDMA_0_INCLUDE_S2MM,
		XPAR_AXIDMA_0_INCLUDE_S2MM_DRE,
#ifdef XPAR_AXIDMA_0_S_AXIS_S2MM_DATA_WIDTH
		XPAR_AXIDMA_0_S_AXIS_S2MM_DATA_WIDTH
#else
		XPAR_AXI_DMA_ETHERNET_S_AXIS_S2MM_TDATA_WIDTH
#endif
	}
};

static XAxiEthernet_Config XAxiEthernet_ConfigTable[] = {
	{
		 XPAR_AXIETHERNET_0_DEVICE_ID,
		 XPAR_AXIETHERNET_0_BASEADDR,
		 XPAR_AXIETHERNET_0_TEMAC_TYPE,
		 XPAR_AXIETHERNET_0_TXCSUM,
		 XPAR_AXIETHERNET_0_RXCSUM,
		 XPAR_AXIETHERNET_0_PHY_TYPE,
		 XPAR_AXIETHERNET_0_TXVLAN_TRAN,
		 XPAR_AXIETHERNET_0_RXVLAN_TRAN,
		 XPAR_AXIETHERNET_0_TXVLAN_TAG,
		 XPAR_AXIETHERNET_0_RXVLAN_TAG,
		 XPAR_AXIETHERNET_0_TXVLAN_STRP,
		 XPAR_AXIETHERNET_0_RXVLAN_STRP,
		 XPAR_AXIETHERNET_0_MCAST_EXTEND,
		 XPAR_AXIETHERNET_0_STATS,
		 XPAR_AXIETHERNET_0_AVB,
		 XPAR_AXIETHERNET_0_INTR,
		 XPAR_AXIETHERNET_0_CONNECTED_TYPE,
		 XPAR_AXIETHERNET_0_CONNECTED_BASEADDR,
		 XPAR_AXIETHERNET_0_CONNECTED_FIFO_INTR,
		 0xFF,
		 0xFF
	}
};

/******************************************************************************/
/**
*
* This is only here because the timer was not working in u-boot when flash is
* in.
*
* For Microblaze we use an assembly loop that is roughly the same regardless of
* optimization level, although caches and memory access time can make the delay
* vary.  Just keep in mind that after resetting or updating the PHY modes,
* the PHY typically needs time to recover.
*
* @return	None
*
* @note		None
*
******************************************************************************/
static void AxiEthernetUtilPhyDelay(unsigned int Seconds)
{
	static int WarningFlag = 0;

	/* If MB caches are disabled or do not exist, this delay loop could
	 * take minutes instead of seconds (e.g., 30x longer).  Print a warning
	 * message for the user (once).  If only MB had a built-in timer!
	 */
	if (!icache_status() && !WarningFlag) {
		WarningFlag = 1;
	}

#define ITERS_PER_SEC   (XPAR_CPU_CORE_CLOCK_FREQ_HZ / 6)
    asm volatile ("\n"
			"1:               \n\t"
			"addik r7, r0, %0 \n\t"
			"2:               \n\t"
			"addik r7, r7, -1 \n\t"
			"bneid  r7, 2b    \n\t"
			"or  r0, r0, r0   \n\t"
			"bneid %1, 1b     \n\t"
			"addik %1, %1, -1 \n\t"
			:: "i"(ITERS_PER_SEC), "d" (Seconds));

}

/* Use MII register 1 (MII status register) to detect PHY */
#define PHY_DETECT_REG  1

/* Mask used to verify certain PHY features (or register contents)
 * in the register above:
 *  0x1000: 10Mbps full duplex support
 *  0x0800: 10Mbps half duplex support
 *  0x0008: Auto-negotiation support
 */
#define PHY_DETECT_MASK 0x1808

/* setting axi emac and phy to proper setting */
static int setup_phy(void)
{
	int i;
	unsigned retries = 100;
	u16 PhyReg;
	u16 PhyReg2;
	int PhyAddr = -1;
	u32 emmc_reg;

	debug("detecting phy address\n");
	
	/* detect the PHY address */
	for (PhyAddr = 31; PhyAddr >= 0; PhyAddr--) {
		XAxiEthernet_PhyRead(&AxiEmac, PhyAddr,
				 	PHY_DETECT_REG, &PhyReg);

		if ((PhyReg != 0xFFFF) &&
		   ((PhyReg & PHY_DETECT_MASK) == PHY_DETECT_MASK)) {
			/* Found a valid PHY address */

			debug("Found valid phy address, %d\n", PhyReg);
			break;
		}
	}

	debug("waiting for the phy to be up\n");

	/* wait for link up and autonegotiation completed */
	while (retries-- > 0) {
		if ((PhyReg & 0x24) == 0x24)
			break;
	}

	/* get PHY id */
	XAxiEthernet_PhyRead(&AxiEmac, PhyAddr, 2, &PhyReg);
	XAxiEthernet_PhyRead(&AxiEmac, PhyAddr, 2, &PhyReg2);
	i = (PhyReg << 16) | PhyReg2;
	debug ("LL_TEMAC: Phy ID 0x%x\n", i);

	/* Marwell 88e1111 id - ml50x, 0x1410141 id - sp605 */

	if ((i == 0x1410cc2) || (i == 0x1410141)) {

		debug("Marvell PHY recognized\n");

		/* Setup the emac for the phy speed */
		emmc_reg = XAxiEthernet_ReadReg(AxiEmac.Config.BaseAddress, 
						XAE_EMMC_OFFSET);
		emmc_reg &= ~XAE_EMMC_LINKSPEED_MASK;

		XAxiEthernet_PhyRead(&AxiEmac, PhyAddr, 17, &PhyReg);

		if((PhyReg & 0x8000) == 0x8000) {
			emmc_reg |= XAE_EMMC_LINKSPD_1000;
			printf("1000BASE-T\n");
		} else if((PhyReg & 0x4000) == 0x4000) {
			printf("100BASE-T\n");
			emmc_reg |= XAE_EMMC_LINKSPD_100;
		} else {
			printf("10BASE-T\n");
			emmc_reg |= XAE_EMMC_LINKSPD_10;
		}

		/* Write new speed setting out to Axi Ethernet */
		XAxiEthernet_WriteReg(AxiEmac.Config.BaseAddress, 
					XAE_EMMC_OFFSET, emmc_reg);

		/*
		 * Setting the operating speed of the MAC needs a delay.  There
		 * doesn't seem to be register to poll, so please consider this
		 * during your application design.
		 */
		AxiEthernetUtilPhyDelay(1);
	}
	return 0;
}

void static axiemac_halt(struct eth_device *dev)
{
	debug("axiemac halted\n");
	
	if (initialized) {
		XAxiEthernet_Stop(&AxiEmac);
		initialized = 0;
	}
}

static int axiemac_init(struct eth_device *dev, bd_t * bis)
{
	int Status;
	XAxiDma_BdRing *RxRingPtr = XAxiDma_GetRxRing(&AxiDma);
	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(&AxiDma);
	XAxiDma_Bd BdTemplate;
	XAxiDma_Bd *Bd1Ptr;

	debug("axi emac init started\n");

	/*************************************/
	/* Setup device for first-time usage */
	/*************************************/

	/*
	 * Initialize AXIDMA engine. AXIDMA engine must be initialized before
	 * AxiEthernet. During AXIDMA engine initialization, AXIDMA hardware is
	 * reset, and since AXIDMA reset line is connected to AxiEthernet, this
	 * would ensure a reset of AxiEthernet.
	 */
	Status = XAxiDma_CfgInitialize(&AxiDma, &XAxiDma_ConfigTable[0]);
	if(Status != XST_SUCCESS) {
		printf("Error initializing DMA\n");
		return 0;
	}

	/*
	 * Initialize AxiEthernet hardware.
	 */
	Status = XAxiEthernet_CfgInitialize(&AxiEmac, &XAxiEthernet_ConfigTable[0], 
						XAxiEthernet_ConfigTable->BaseAddress);

	if (Status != XST_SUCCESS) {
		printf("Error in initialize\n");
		return 0;
	}

	/*
	 * Set the MAC address
	 */
	Status = XAxiEthernet_SetMacAddress(&AxiEmac, &dev->enetaddr[0]);
	if (Status != XST_SUCCESS) {
		printf("Error setting MAC address\n");
		return 0;
	}

	/*
	 * Setup RxBD space.
	 *
	 * We have already defined a properly aligned area of memory to store
	 * RxBDs at the beginning of this source code file so just pass its
	 * address into the function. No MMU is being used so the physical and
	 * virtual addresses are the same.
	 *
	 * Setup a BD template for the Rx channel. This template will be
	 * copied to every RxBD. We will not have to explicitly set these
	 * again.
	 */

	/*
	 * Disable all RX interrupts before RxBD space setup
	 */
	XAxiDma_BdRingIntDisable(RxRingPtr, XAXIDMA_IRQ_ALL_MASK);

	/*
	 * Create the RxBD ring
	 */
	Status = XAxiDma_BdRingCreate(RxRingPtr, (u32) &RxBdSpace,
				     (u32) &RxBdSpace, BD_ALIGNMENT, RXBD_CNT);
	if (Status != XST_SUCCESS) {
		printf("Error setting up RxBD space\n");
		return 0;
	}

	XAxiDma_BdClear(&BdTemplate);
	Status = XAxiDma_BdRingClone(RxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {
		printf("Error initializing RxBD space\n");
		return 0;
	}

	/*
	 * Setup TxBD space.
	 *
	 * Like RxBD space, we have already defined a properly aligned area of
	 * memory to use.
	 */

	/*
	 * Create the TxBD ring
	 */
	Status = XAxiDma_BdRingCreate(TxRingPtr, (u32) &TxBdSpace,
				(u32) &TxBdSpace, BD_ALIGNMENT, TXBD_CNT);
	if (Status != XST_SUCCESS) {
		printf("Error setting up TxBD space\n");
		return 0;
	}
	/*
	 * We reuse the bd template, as the same one will work for both rx
	 * and tx.
	 */
	Status = XAxiDma_BdRingClone(TxRingPtr, &BdTemplate);
	if (Status != XST_SUCCESS) {
		printf("Error initializing TxBD space\n");
		return 0;
	}

	/*
	 * Make sure Tx and Rx are enabled
	 */
	Status = XAxiEthernet_SetOptions(&AxiEmac,
					     XAE_RECEIVER_ENABLE_OPTION |
					     XAE_TRANSMITTER_ENABLE_OPTION);
	if (Status != XST_SUCCESS) {
		printf("Error setting options\n");
		return 0;
	}

	/*
	 * Start the Axi Ethernet device and enable the ERROR interrupt
	 */
	XAxiEthernet_Start(&AxiEmac);

	/*
	 * Allocate 1 RxBD.
	 */
	Status = XAxiDma_BdRingAlloc(RxRingPtr, 1, &Bd1Ptr);
	if (Status != XST_SUCCESS) {
		printf("Error allocating RxBD\n");
		return 0;
	}

	/*
	 * Setup the BD.
	 */
	XAxiDma_BdSetBufAddr(Bd1Ptr, (u32)&RxFrame);
	XAxiDma_BdSetLength(Bd1Ptr, sizeof(RxFrame));
	XAxiDma_BdSetCtrl(Bd1Ptr, 0);

	/*
	 * Enqueue to HW
	 */
	Status = XAxiDma_BdRingToHw(RxRingPtr, 1, Bd1Ptr);
	if (Status != XST_SUCCESS) {
		printf("Error committing RxBD to HW\n");
		return 0;
	}

	/*
	 * Start DMA RX channel. Now it's ready to receive data.
	 */
	Status = XAxiDma_BdRingStart(RxRingPtr);
	if (Status != XST_SUCCESS) {
		return 0;
	}

	setup_phy();

	initialized = 1;

	debug("axi emac init complete\n");

	/* success */

	return 1;
}

static int IsTxDone(XAxiDma_BdRing *TxRingPtr)
{
	u32 IrqStatus;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_BdRingGetIrq(TxRingPtr);

	/* Acknowledge pending interrupts */
	XAxiDma_BdRingAckIrq(TxRingPtr, IrqStatus);
	/*
	 * If no interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if (!(IrqStatus & XAXIDMA_IRQ_ALL_MASK)) {
		printf("AXIDma: No interrupts asserted in TX status register\n");
		XAxiDma_Reset(&AxiDma);
		if(!XAxiDma_ResetIsDone(&AxiDma)) {
			printf("AxiDMA: Error: Could not reset\n");
		}
		return 0;
	}

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		printf("AXIDMA: TX Error interrupts\n");

		/* Reset should never fail for transmit channel
		 */
		XAxiDma_Reset(&AxiDma);
		if(!XAxiDma_ResetIsDone(&AxiDma)) {

			printf("AXIDMA: Error: Could not reset\n");
		}

		return 0;
	}

	/*
	 * If Transmit done interrupt is asserted, call TX call back function
	 * to handle the processed BDs and raise the according flag
	 */
	if ((IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))) {
		return 1;
	}

	return 0;
}

static int IsRxReady(XAxiDma_BdRing *RxRingPtr)
{
	u32 IrqStatus;

	/* Read pending interrupts */
	IrqStatus = XAxiDma_BdRingGetIrq(RxRingPtr);

	/* Acknowledge pending interrupts */
	XAxiDma_BdRingAckIrq(RxRingPtr, IrqStatus);

	if (IrqStatus) debug("axi emac irq status = %08X\n", IrqStatus);

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((IrqStatus & XAXIDMA_IRQ_ERROR_MASK)) {

		printf("AXIDMA: RX Error interrupts\n");

		/*
		 * Reset could fail and hang
		 * NEED a way to handle this or do not call it??
		 */
		XAxiDma_Reset(&AxiDma);

		if(!XAxiDma_ResetIsDone(&AxiDma)) {
			printf("AXIDMA: Could not reset\n");
		}
		return 0;
	}
	/*
	 * If Reception done interrupt is asserted, call RX call back function
	 * to handle the processed BDs and then raise the according flag.
	 */
	if ((IrqStatus & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))) {	
		return 1;
	}

	return 0;
}

static int axiemac_send (struct eth_device *dev, volatile void *ptr, int len)
{
	int Status;
	XAxiDma_BdRing *TxRingPtr = XAxiDma_GetTxRing(&AxiDma);
	XAxiDma_Bd *Bd1Ptr;

	debug("axi emac send starting\n");

	if (len > ENET_MAX_MTU)
		len = ENET_MAX_MTU;

	/*
	 * Allocate, setup, and enqueue a TxBD. 
	 */
	Status = XAxiDma_BdRingAlloc(TxRingPtr, 1, &Bd1Ptr);
	if (Status != XST_SUCCESS) {
		printf("Error allocating TxBD\n");
		return XST_FAILURE;
	}

	/*
	 * Setup TxBD 
	 */
	XAxiDma_BdSetBufAddr(Bd1Ptr, (u32)ptr);
	XAxiDma_BdSetLength(Bd1Ptr, len);
	XAxiDma_BdSetCtrl(Bd1Ptr, XAXIDMA_BD_CTRL_TXSOF_MASK |
				XAXIDMA_BD_CTRL_TXEOF_MASK);

	/*
	 * Enqueue to HW
	 */
	Status = XAxiDma_BdRingToHw(TxRingPtr, 1, Bd1Ptr);
	if (Status != XST_SUCCESS) {
		/*
		 * Undo BD allocation and exit
		 */
		XAxiDma_BdRingUnAlloc(TxRingPtr, 1, Bd1Ptr);
		printf("Error committing TxBD to HW\n");
		return 0;
	}

	/*
	 * Start DMA TX channel. Transmission starts at once.
	 */
	Status = XAxiDma_BdRingStart(TxRingPtr);
	if (Status != XST_SUCCESS) {
		return 0;
	}

	/*
	 * Wait for transmission to complete
	 */

	debug("axi emac, waiting for tx to be done\n");

	while (!IsTxDone(TxRingPtr));

	debug("axi emac, tx done\n");

	/*
	 * Now that the frame has been sent, post process our TxBDs.
	 * Since we have only submitted 2 to HW, then there should be only 2 ready
	 * for post processing.
	 */
	if (XAxiDma_BdRingFromHw(TxRingPtr, 1, &Bd1Ptr) == 0) {
		printf("TxBDs were not ready for post processing\n");
		return 0;
	}

	/*
	 * Examine the TxBDs.
	 *
	 * There isn't much to do. The only thing to check would be DMA exception
	 * bits. But this would also be caught in the error handler. So we just
	 * return these BDs to the free list
	 */
	Status = XAxiDma_BdRingFree(TxRingPtr, 1, Bd1Ptr);
	if (Status != XST_SUCCESS) {
		printf("Error freeing up TxBDs\n");
		return 0;
	}

	debug("axi emac send complete\n");

	return 1;
}

static int axiemac_recv(struct eth_device *dev)
{
	int Status;
	XAxiDma_BdRing *RxRingPtr = XAxiDma_GetRxRing(&AxiDma);
	XAxiDma_Bd *Bd1Ptr;
	XAxiDma_Bd *BdCurPtr;
	u32 BdSts;	
	u32 RxFrameLength;

	/*
	 * Wait for Rx indication
	 */
	if (!IsRxReady(RxRingPtr)) {
		return 0;
	}

	debug("axi emac, rx data ready\n");

	/*
	 * Now that the frame has been received, post process our RxBD.
	 * Since we have only submitted 1 to HW, then there should be only 1
	 * ready for post processing.
	 */
	if (XAxiDma_BdRingFromHw(RxRingPtr, 1, &Bd1Ptr) == 0) {
		printf("RxBD was not ready for post processing\n");
		return 0;
	}
	BdCurPtr = Bd1Ptr;
	BdSts = XAxiDma_BdGetSts(BdCurPtr);
	if ((BdSts & XAXIDMA_BD_STS_ALL_ERR_MASK) ||
		(!(BdSts & XAXIDMA_BD_STS_COMPLETE_MASK))) {
			printf("Rx Error\n");
		return 0;
	}
	else {
		RxFrameLength =
		(XAxiDma_BdRead(BdCurPtr,XAXIDMA_BD_USR4_OFFSET)) & 0x0000FFFF;

#ifdef PACKET_DUMP
		print_buffer (&RxFrame, &RxFrame[0], 1, RxFrameLength, 16);
#endif

		/* pass the received frame up for processing */
		if (RxFrameLength) NetReceive(RxFrame, RxFrameLength);

		XAxiDma_BdSetBufAddr(Bd1Ptr, (u32)&RxFrame);
		XAxiDma_BdSetLength(Bd1Ptr, sizeof(RxFrame));
		XAxiDma_BdSetCtrl(Bd1Ptr, 0);

		/*
		 * Return the RxBD back to the channel for later allocation. Free the
		 * exact number we just post processed.
		 */
		Status = XAxiDma_BdRingFree(RxRingPtr, 1, Bd1Ptr);
		if (Status != XST_SUCCESS) {
			printf("Error freeing up RxBDs\n");
			return 0;
		}

		/*
		 * Allocate 1 RxBD.
		 */
		Status = XAxiDma_BdRingAlloc(RxRingPtr, 1, &Bd1Ptr);
		if (Status != XST_SUCCESS) {
			printf("Error allocating RxBD\n");
			return 0;
		}

		/*
		 * Enqueue to HW
		 */
		Status = XAxiDma_BdRingToHw(RxRingPtr, 1, Bd1Ptr);
		if (Status != XST_SUCCESS) {
			printf("Error committing RxBD to HW\n");
			return 0;
		}
	}

	debug("axi emac rx complete, framelength = %d\n", RxFrameLength);

	return RxFrameLength;
}

int xilinx_axiemac_initialize (bd_t *bis, int base_addr)
{
	struct eth_device *dev;

	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		hang();

	memset(dev, 0, sizeof(*dev));
	sprintf(dev->name, "Xilinx_AxiEmac");

	dev->iobase = base_addr;
	dev->priv = 0;
	dev->init = axiemac_init;
	dev->halt = axiemac_halt;
	dev->send = axiemac_send;
	dev->recv = axiemac_recv;

	eth_register(dev);

	return 0;
}
