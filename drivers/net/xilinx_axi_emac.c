/*
 * Copyright (C) 2011 Michal Simek <monstr@monstr.eu>
 * Copyright (C) 2011 PetaLogix
 * Copyright (C) 2010 Xilinx, Inc. All rights reserved.
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <config.h>
#include <common.h>
#include <net.h>
#include <malloc.h>
#include <asm/io.h>

/* Axi Ethernet registers offset */
#define XAE_IS_OFFSET		0x0000000C /* Interrupt status */
#define XAE_IE_OFFSET		0x00000014 /* Interrupt enable */
#define XAE_RCW1_OFFSET		0x00000404 /* Rx Configuration Word 1 */
#define XAE_TC_OFFSET		0x00000408 /* Tx Configuration */
#define XAE_EMMC_OFFSET		0x00000410 /* EMAC mode configuration */
#define XAE_MDIO_MC_OFFSET	0x00000500 /* MII Management Config */
#define XAE_MDIO_MCR_OFFSET	0x00000504 /* MII Management Control */
#define XAE_MDIO_MRD_OFFSET	0x0000050C /* MII Management Read Data */

/* Link setup */
#define XAE_EMMC_LINKSPEED_MASK	0xC0000000 /* Link speed */
#define XAE_EMMC_LINKSPD_10	0x00000000 /* Link Speed mask for 10 Mbit */
#define XAE_EMMC_LINKSPD_100	0x40000000 /* Link Speed mask for 100 Mbit */
#define XAE_EMMC_LINKSPD_1000	0x80000000 /* Link Speed mask for 1000 Mbit */

/* Interrupt Status/Enable/Mask Registers bit definitions */
#define XAE_INT_RXRJECT_MASK	0x00000008 /* Rx frame rejected */
#define XAE_INT_MGTRDY_MASK	0x00000080 /* MGT clock Lock */

/* Receive Configuration Word 1 (RCW1) Register bit definitions */
#define XAE_RCW1_RX_MASK	0x10000000 /* Receiver enable */

/* Transmitter Configuration (TC) Register bit definitions */
#define XAE_TC_TX_MASK		0x10000000 /* Transmitter enable */

/*
 * Station address bits [47:32]
 * Station address bits [31:0] are stored in register UAW0
 */
#define XAE_UAW0_OFFSET			0x00000700 /* Unicast address word 0 */
#define XAE_UAW1_OFFSET			0x00000704 /* Unicast address word 1 */
#define XAE_UAW1_UNICASTADDR_MASK	0x0000FFFF

/* MDIO Management Configuration (MC) Register bit definitions */
#define XAE_MDIO_MC_MDIOEN_MASK		0x00000040 /* MII management enable*/

/* MDIO Management Control Register (MCR) Register bit definitions */
#define XAE_MDIO_MCR_PHYAD_MASK		0x1F000000 /* Phy Address Mask */
#define XAE_MDIO_MCR_PHYAD_SHIFT	24	   /* Phy Address Shift */
#define XAE_MDIO_MCR_REGAD_MASK		0x001F0000 /* Reg Address Mask */
#define XAE_MDIO_MCR_REGAD_SHIFT	16	   /* Reg Address Shift */
#define XAE_MDIO_MCR_OP_READ_MASK	0x00008000 /* Op Code Read Mask */
#define XAE_MDIO_MCR_OP_WRITE_MASK	0x00004000 /* Op Code Write Mask */
#define XAE_MDIO_MCR_INITIATE_MASK	0x00000800 /* Ready Mask */
#define XAE_MDIO_MCR_READY_MASK		0x00000080 /* Ready Mask */

#define XAE_MDIO_DIV_DFT	29	/* Default MDIO clock divisor */

/* DMA macros */
/* Bitmasks of XAXIDMA_CR_OFFSET register */
#define XAXIDMA_CR_RUNSTOP_MASK	0x00000001 /* Start/stop DMA channel */
#define XAXIDMA_CR_RESET_MASK	0x00000004 /* Reset DMA engine */

/* Bitmasks of XAXIDMA_SR_OFFSET register */
#define XAXIDMA_HALTED_MASK	0x00000001  /* DMA channel halted */

/* Bitmask for interrupts */
#define XAXIDMA_IRQ_IOC_MASK	0x00001000 /* Completion intr */
#define XAXIDMA_IRQ_DELAY_MASK	0x00002000 /* Delay interrupt */
#define XAXIDMA_IRQ_ALL_MASK	0x00007000 /* All interrupts */

/* Bitmasks of XAXIDMA_BD_CTRL_OFFSET register */
#define XAXIDMA_BD_CTRL_TXSOF_MASK	0x08000000 /* First tx packet */
#define XAXIDMA_BD_CTRL_TXEOF_MASK	0x04000000 /* Last tx packet */


#define ENET_MAX_MTU PKTSIZE_ALIGN
#define DMAALIGN	128

static u8 RxFrame[ENET_MAX_MTU] __attribute((aligned(DMAALIGN))) ;

/* reflect dma offsets */
struct axidma_reg {
	u32 control; /* DMACR */
	u32 status; /* DMASR */
	u32 current; /* CURDESC */
	u32 reserved;
	u32 tail; /* TAILDESC */
};

/* Private driver structures */
struct axidma_priv {
	struct axidma_reg *dmatx;
	struct axidma_reg *dmarx;
};

/* BD descriptors */
typedef struct axidma_bd {
	u32 next;	/* Next descriptor pointer */
	u32 reserved1;
	u32 phys;	/* Buffer address */
	u32 reserved2;
	u32 reserved3;
	u32 reserved4;
	u32 cntrl;	/* Control */
	u32 status;	/* Status */
	u32 app0;
	u32 app1;	/* TX start << 16 | insert */
	u32 app2;	/* TX csum seed */
	u32 app3;
	u32 app4;
	u32 sw_id_offset;
	u32 reserved5;
	u32 reserved6;
} axidma_bd __attribute((aligned(DMAALIGN)));

/* Static BDs - driver uses only one BD */
static axidma_bd tx_bd;
static axidma_bd rx_bd;

/* Use MII register 1 (MII status register) to detect PHY */
#define PHY_DETECT_REG  1

/* Mask used to verify certain PHY features (or register contents)
 * in the register above:
 *  0x1000: 10Mbps full duplex support
 *  0x0800: 10Mbps half duplex support
 *  0x0008: Auto-negotiation support
 */
#define PHY_DETECT_MASK 0x1808

static u16 phy_read(struct eth_device *dev, u32 PhyAddress, u32 RegisterNum)
{
	u32 MdioCtrlReg = 0;

	/* Wait till MDIO interface is ready to accept a new transaction. */
	while (!(in_be32(dev->iobase + XAE_MDIO_MCR_OFFSET)
						& XAE_MDIO_MCR_READY_MASK))
		;

	MdioCtrlReg =   ((PhyAddress << XAE_MDIO_MCR_PHYAD_SHIFT) &
			XAE_MDIO_MCR_PHYAD_MASK) |
			((RegisterNum << XAE_MDIO_MCR_REGAD_SHIFT)
			& XAE_MDIO_MCR_REGAD_MASK) |
			XAE_MDIO_MCR_INITIATE_MASK |
			XAE_MDIO_MCR_OP_READ_MASK;

	out_be32(dev->iobase + XAE_MDIO_MCR_OFFSET, MdioCtrlReg);

	/* Wait till MDIO transaction is completed. */
	while (!(in_be32(dev->iobase + XAE_MDIO_MCR_OFFSET)
						& XAE_MDIO_MCR_READY_MASK))
		;

	/* Read data */
	return (u16) in_be32(dev->iobase + XAE_MDIO_MRD_OFFSET);
}

/* setting axi emac and phy to proper setting */
static void setup_phy(struct eth_device *dev)
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
		PhyReg = phy_read(dev, PhyAddr, PHY_DETECT_REG);

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
	PhyReg = phy_read(dev, PhyAddr, 2);
	PhyReg2 = phy_read(dev, PhyAddr, 2);
	i = (PhyReg << 16) | PhyReg2;
	debug("LL_TEMAC: Phy ID 0x%x\n", i);

	/* Marwell 88e1111 id - ml50x, 0x1410141 id - sp605 */
	if ((i == 0x1410cc2) || (i == 0x1410141)) {
		debug("Marvell PHY recognized\n");

		/* Setup the emac for the phy speed */
		emmc_reg = in_be32(dev->iobase + XAE_EMMC_OFFSET);
		emmc_reg &= ~XAE_EMMC_LINKSPEED_MASK;

		PhyReg = phy_read(dev, PhyAddr, 17);

		if ((PhyReg & 0x8000) == 0x8000) {
			emmc_reg |= XAE_EMMC_LINKSPD_1000;
			printf("1000BASE-T\n");
		} else if ((PhyReg & 0x4000) == 0x4000) {
			printf("100BASE-T\n");
			emmc_reg |= XAE_EMMC_LINKSPD_100;
		} else {
			printf("10BASE-T\n");
			emmc_reg |= XAE_EMMC_LINKSPD_10;
		}

		/* Write new speed setting out to Axi Ethernet */
		out_be32(dev->iobase + XAE_EMMC_OFFSET, emmc_reg);

		/*
		 * Setting the operating speed of the MAC needs a delay. There
		 * doesn't seem to be register to poll, so please consider this
		 * during your application design.
		 */
		udelay(1);
	}
}

/* STOP DMA transfers */
static void axiemac_halt(struct eth_device *dev)
{
	struct axidma_priv *dma = dev->priv;

	if (dev->state) {
		/* Stop the hardware */
		dma->dmatx->control &= ~XAXIDMA_CR_RUNSTOP_MASK;
		dma->dmarx->control &= ~XAXIDMA_CR_RUNSTOP_MASK;
		dev->state = 0;
	}
	debug("axiemac halted\n");
}

static void axi_ethernet_init(struct eth_device *dev)
{
	/*
	 * Check the status of the MgtRdy bit in the interrupt status
	 * registers. This must be done to allow the MGT clock to become stable
	 * for the Sgmii and 1000BaseX PHY interfaces. No other register reads
	 * will be valid until this bit is valid.
	 * The bit is always a 1 for all other PHY interfaces.
	 */
	u32 timeout = 200;
	while (timeout &&
		(!(in_be32(dev->iobase + XAE_IS_OFFSET) & XAE_INT_MGTRDY_MASK)))
		timeout--;

	if (!timeout)
		printf("%s: Timeout\n", __func__);

	/* Stop the device and reset HW */
	/* Disable interrupts */
	out_be32(dev->iobase + XAE_IE_OFFSET, 0);

	/* Disable the receiver */
	out_be32(dev->iobase + XAE_RCW1_OFFSET,
		in_be32(dev->iobase + XAE_RCW1_OFFSET) & ~XAE_RCW1_RX_MASK);

	/*
	 * Stopping the receiver in mid-packet causes a dropped packet
	 * indication from HW. Clear it.
	 */
	/* set the interrupt status register to clear the interrupt */
	out_be32(dev->iobase + XAE_IS_OFFSET, XAE_INT_RXRJECT_MASK);

	/* Setup HW */
	/* Set default MDIO divisor */
	out_be32(dev->iobase + XAE_MDIO_MC_OFFSET,
			(u32) XAE_MDIO_DIV_DFT | XAE_MDIO_MC_MDIOEN_MASK);

	debug("XAxiEthernet InitHw: done\n");
}

static void setup_mac(struct eth_device *dev)
{
	/* Set the MAC address */
	int val = ((dev->enetaddr[3] << 24) | (dev->enetaddr[2] << 16) |
		(dev->enetaddr[1] << 8) | (dev->enetaddr[0]));
	out_be32(dev->iobase + XAE_UAW0_OFFSET, val);

	val = (dev->enetaddr[5] << 8) | dev->enetaddr[4] ;
	val |= in_be32(dev->iobase + XAE_UAW1_OFFSET)
						& ~XAE_UAW1_UNICASTADDR_MASK;
	out_be32(dev->iobase + XAE_UAW1_OFFSET, val);
}

/* Reset DMA engine */
static void axi_dma_init(struct eth_device *dev)
{
	struct axidma_priv *dma = dev->priv;

	/* Reset the engine so the hardware starts from a known state */
	dma->dmatx->control = XAXIDMA_CR_RESET_MASK;
	dma->dmarx->control = XAXIDMA_CR_RESET_MASK;

	/* At the initialization time, hardware should finish reset quickly */
	u32 timeout = 500;
	while (timeout--) {
		/* Check transmit/receive channel */
		/* Reset is done when the reset bit is low */
		if (!(dma->dmatx->control | dma->dmarx->control)
						& XAXIDMA_CR_RESET_MASK)
			break;
		timeout -= 1;
	}

	if (!timeout)
		printf("%s: Timeout\n", __func__);
}

static int axiemac_init(struct eth_device *dev, bd_t * bis)
{
	struct axidma_priv *dma = dev->priv;
	debug("axi emac init started\n");

	/*
	 * Initialize AXIDMA engine. AXIDMA engine must be initialized before
	 * AxiEthernet. During AXIDMA engine initialization, AXIDMA hardware is
	 * reset, and since AXIDMA reset line is connected to AxiEthernet, this
	 * would ensure a reset of AxiEthernet.
	 */
	axi_dma_init(dev);

	/* Initialize AxiEthernet hardware. */
	axi_ethernet_init(dev);
	setup_mac(dev);

	/* Start the hardware */
	dma->dmarx->control |= XAXIDMA_CR_RUNSTOP_MASK;
	/* Start DMA RX channel. Now it's ready to receive data.*/
	dma->dmarx->current = (u32)&rx_bd;

	/* Disable all RX interrupts before RxBD space setup */
	dma->dmarx->control &= ~(XAXIDMA_IRQ_ALL_MASK & XAXIDMA_IRQ_ALL_MASK);

	/* Setup the BD. */
	memset((void *) &rx_bd, 0, sizeof(axidma_bd));
	rx_bd.next = (u32)&rx_bd;
	rx_bd.phys = (u32)&RxFrame;
	rx_bd.cntrl = sizeof(RxFrame);
	/* Flush the last BD so DMA core could see the updates */
	flush_cache((u32)&rx_bd, sizeof(axidma_bd));

	/* it is necessary to flush RxFrame because if you don't do it
	 * then cache can contain uninitialized data */
	flush_cache((u32)&RxFrame, sizeof(RxFrame));

	/* Rx BD is ready - start */
	dma->dmarx->tail = (u32)&rx_bd;

	/* enable TX */
	out_be32(dev->iobase + XAE_TC_OFFSET, XAE_TC_TX_MASK);
	/* enable RX */
	out_be32(dev->iobase + XAE_RCW1_OFFSET, XAE_RCW1_RX_MASK);

	/* PHY setup */
	setup_phy(dev);
	dev->state = 1;
	debug("axi emac init complete\n");
	return 1;
}

static int axiemac_send(struct eth_device *dev, volatile void *ptr, int len)
{
	struct axidma_priv *dma = dev->priv;

	if (len > ENET_MAX_MTU)
		len = ENET_MAX_MTU;

	/* Flush packet to main memory to be trasfered by DMA */
	flush_cache(ptr, len);

	/* Setup Tx BD */
	memset((void *) &tx_bd, 0, sizeof(axidma_bd));
	/* At the end of the ring, link the last BD back to the top */
	tx_bd.next = (u32)&tx_bd;
	tx_bd.phys = (u32)ptr;
	/* save len */
	tx_bd.cntrl = len | XAXIDMA_BD_CTRL_TXSOF_MASK |
						XAXIDMA_BD_CTRL_TXEOF_MASK;

	/* Flush the last BD so DMA core could see the updates */
	flush_cache((u32)&tx_bd, sizeof(axidma_bd));

	if (dma->dmatx->status & XAXIDMA_HALTED_MASK) {
		dma->dmatx->current = (u32)&tx_bd;
		/* Start the hardware */
		dma->dmatx->control |= XAXIDMA_CR_RUNSTOP_MASK;
	}

	/* start transfer */
	dma->dmatx->tail = (u32)&tx_bd;

	/* Wait for transmission to complete */
	debug("axi emac, waiting for tx to be done\n");
	while (!dma->dmatx->status &
				(XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK))
		;

	debug("axi emac send complete\n");
	return 1;
}

static int IsRxReady(struct eth_device *dev)
{
	u32 status;
	struct axidma_priv *dma = dev->priv;

	/* Read pending interrupts */
	status = dma->dmarx->status;

	/* Acknowledge pending interrupts */
	dma->dmarx->status &= XAXIDMA_IRQ_ALL_MASK;

	/*
	 * If Reception done interrupt is asserted, call RX call back function
	 * to handle the processed BDs and then raise the according flag.
	 */
	if ((status & (XAXIDMA_IRQ_DELAY_MASK | XAXIDMA_IRQ_IOC_MASK)))
		return 1;

	return 0;
}

static int axiemac_recv(struct eth_device *dev)
{
	u32 length;
	struct axidma_priv *dma = dev->priv;

	/* wait for an incoming packet */
	if (!IsRxReady(dev))
		return 0;

	debug("axi emac, rx data ready\n");

	/* Disable IRQ for a moment till packet is handled */
	dma->dmarx->control &= ~(XAXIDMA_IRQ_ALL_MASK & XAXIDMA_IRQ_ALL_MASK);

	length = rx_bd.app4 & 0x0000FFFF;
#ifdef DEBUG
	print_buffer(&RxFrame, &RxFrame[0], 1, length, 16);
#endif
	/* pass the received frame up for processing */
	if (length)
		NetReceive(RxFrame, length);

#ifdef DEBUG
	/* It is useful to clear buffer to be sure that it is consistent */
	memset(RxFrame, 0, sizeof(RxFrame));
#endif
	/* Setup RxBD */
	/* Clear the whole buffer and setup it again - all flags are cleared */
	memset((void *) &rx_bd, 0, sizeof(axidma_bd));
	rx_bd.next = (u32)&rx_bd;
	rx_bd.phys = (u32)&RxFrame;
	rx_bd.cntrl = sizeof(RxFrame);

	/* write bd to HW */
	flush_cache((u32)&rx_bd, sizeof(axidma_bd));

	/* it is necessary to flush RxFrame because if you don't do it
	 * then cache will contain previous packet */
	flush_cache((u32)&RxFrame, sizeof(RxFrame));

	/* Rx BD is ready - start again */
	dma->dmarx->tail = (u32)&rx_bd;

	debug("axi emac rx complete, framelength = %d\n", length);

	return length;
}

int xilinx_axiemac_initialize(bd_t *bis, int base_addr, int dma_addr)
{
	struct eth_device *dev;
	struct axidma_priv *dma;

	dev = malloc(sizeof(*dev));
	if (dev == NULL)
		hang();

	memset(dev, 0, sizeof(*dev));
	sprintf(dev->name, "Xilinx_AxiEmac");

	dev->iobase = base_addr;
	dma = dev->priv;
	dma->dmatx = dma_addr;
	dma->dmarx = (u32)dma->dmatx + 0x30; /* rx channel offset */
	dev->init = axiemac_init;
	dev->halt = axiemac_halt;
	dev->send = axiemac_send;
	dev->recv = axiemac_recv;

	eth_register(dev);

	return 0;
}
