/*
 * (C) Copyright 2011 Michal Simek
 *
 * Michal SIMEK <monstr@monstr.eu>
 *
 * Based on Xilinx gmac driver:
 * (C) Copyright 2011 Xilinx
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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <net.h>
#include <config.h>
#include <malloc.h>
#include <asm/io.h>

#define PHY_ADDR 0x17
#define RX_BUF 3

/* Register offsets */
#define XEMACPSS_NWCTRL_OFFSET        0x00000000 /* Network Control reg */
#define XEMACPSS_NWCFG_OFFSET         0x00000004 /* Network Config reg */
#define XEMACPSS_NWSR_OFFSET          0x00000008 /* Network Status reg */
#define XEMACPSS_DMACR_OFFSET         0x00000010 /* DMA Control reg */
#define XEMACPSS_TXSR_OFFSET          0x00000014 /* TX Status reg */
#define XEMACPSS_RXQBASE_OFFSET       0x00000018 /* RX Q Base address reg */
#define XEMACPSS_TXQBASE_OFFSET       0x0000001C /* TX Q Base address reg */
#define XEMACPSS_RXSR_OFFSET          0x00000020 /* RX Status reg */
#define XEMACPSS_IDR_OFFSET           0x0000002C /* Interrupt Disable reg */
#define XEMACPSS_PHYMNTNC_OFFSET      0x00000034 /* Phy Maintaince reg */

/* Bit/mask specification */
#define XEMACPSS_PHYMNTNC_OP_MASK    0x40020000	/* operation mask bits */
#define XEMACPSS_PHYMNTNC_OP_R_MASK  0x20000000	/* read operation */
#define XEMACPSS_PHYMNTNC_OP_W_MASK  0x10000000	/* write operation */
#define XEMACPSS_PHYMNTNC_PHYAD_SHIFT_MASK   23	/* Shift bits for PHYAD */
#define XEMACPSS_PHYMNTNC_PHREG_SHIFT_MASK   18	/* Shift bits for PHREG */

#define XEMACPSS_RXBUF_EOF_MASK       0x00008000 /* End of frame. */
#define XEMACPSS_RXBUF_SOF_MASK       0x00004000 /* Start of frame. */
#define XEMACPSS_RXBUF_LEN_MASK       0x00003FFF /* Mask for length field */

#define XEMACPSS_RXBUF_WRAP_MASK      0x00000002 /* Wrap bit, last BD */
#define XEMACPSS_RXBUF_NEW_MASK       0x00000001 /* Used bit.. */
#define XEMACPSS_RXBUF_ADD_MASK       0xFFFFFFFC /* Mask for address */

#define XEMACPSS_TXBUF_WRAP_MASK  0x40000000 /* Wrap bit, last descriptor */
#define XEMACPSS_TXBUF_LAST_MASK  0x00008000 /* Last buffer */

#define XEMACPSS_TXSR_HRESPNOK_MASK    0x00000100 /* Transmit hresp not OK */
#define XEMACPSS_TXSR_URUN_MASK        0x00000040 /* Transmit underrun */
#define XEMACPSS_TXSR_BUFEXH_MASK      0x00000010 /* Transmit buffs exhausted
                                                       mid frame */

#define XEMACPSS_NWCTRL_TXEN_MASK        0x00000008 /* Enable transmit */
#define XEMACPSS_NWCTRL_RXEN_MASK        0x00000004 /* Enable receive */
#define XEMACPSS_NWCTRL_MDEN_MASK        0x00000010 /* Enable MDIO port */
#define XEMACPSS_NWCTRL_STARTTX_MASK     0x00000200 /* Start tx (tx_go) */

#define XEMACPSS_NWSR_MDIOIDLE_MASK     0x00000004 /* PHY management idle */

/* BD descriptors */
typedef struct emac_bd {
	u32 addr;	/* Next descriptor pointer */
	u32 status;
} emac_bd;

/* initialized, rxbd_current, rx_first_buf must be 0 after init */
struct gemac_priv {
	emac_bd tx_bd;
	emac_bd rx_bd[RX_BUF];
	u32 initialized;
	char rxbuffers[RX_BUF * PKTSIZE_ALIGN];
	u32 rxbd_current;
	u32 rx_first_buf;
};

/* Just helping function because of arm io.h */
static inline void out32(u32 addr, u32 offset, u32 val)
{
    *(volatile u32 *) (addr + offset) = val;
}

static inline u32 in32(u32 addr, u32 offset)
{
    return *(volatile u32 *) (addr + offset);
}

static u32 phy_setup_op(struct eth_device *dev, u32 regnum, u32 op, u32 data)
{
	u32 mgtcr;

	while (!(in32(dev->iobase, XEMACPSS_NWSR_OFFSET) &
						XEMACPSS_NWSR_MDIOIDLE_MASK))
		;

	/* Construct mgtcr mask for the operation */
	mgtcr = XEMACPSS_PHYMNTNC_OP_MASK | op |
		(PHY_ADDR << XEMACPSS_PHYMNTNC_PHYAD_SHIFT_MASK) |
		(regnum << XEMACPSS_PHYMNTNC_PHREG_SHIFT_MASK) | data;

	/* Write mgtcr and wait for completion */
	out32(dev->iobase, XEMACPSS_PHYMNTNC_OFFSET, mgtcr);

	while (!(in32(dev->iobase, XEMACPSS_NWSR_OFFSET) &
						XEMACPSS_NWSR_MDIOIDLE_MASK))
		;

	if(op == XEMACPSS_PHYMNTNC_OP_R_MASK)
		return in32(dev->iobase, XEMACPSS_PHYMNTNC_OFFSET);

	return 0;
}

static u32 phy_rd(struct eth_device *dev, u32 regnum)
{
	return phy_setup_op(dev, regnum, XEMACPSS_PHYMNTNC_OP_R_MASK, 0);
}

static void phy_wr(struct eth_device *dev, u32 regnum, u32 data)
{
	phy_setup_op(dev, regnum, XEMACPSS_PHYMNTNC_OP_W_MASK, data);
}

static void phy_rst(struct eth_device *dev)
{
	int tmp;

	puts("Resetting PHY...\n");
	tmp = phy_rd(dev, 0);
	tmp |= 0x8000;
	phy_wr(dev, 0, tmp);

	while (phy_rd(dev, 0) & 0x8000) {
   	     putc('.');
	}
	puts("\nPHY reset complete.\n");
}

#define MAC 0

#if MAC
#define XEMACPSS_LAST_OFFSET          0x000001B4 /* Last statistic counter
						      offset, for clearing */

#define XEMACPSS_OCTTXL_OFFSET        0x00000100 /* Octects transmitted Low
                                                      reg */

#define XEMACPSS_HASHL_OFFSET         0x00000080 /* Hash Low address reg */
#define XEMACPSS_HASHH_OFFSET         0x00000084 /* Hash High address reg */

#define XEMACPSS_LADDR1L_OFFSET       0x00000088 /* Specific1 addr low reg */
#define XEMACPSS_LADDR1H_OFFSET       0x0000008C /* Specific1 addr high reg */

#define XEMACPSS_MATCH1_OFFSET        0x000000A8 /* Type ID1 Match reg */

#define XEMACPSS_LADDR_MACH_MASK        0x0000FFFF /* Address bits[47:32]
                                                      bit[31:0] are in BOTTOM */

void SetMacAddress(struct eth_device *dev, void *AddressPtr, u8 Index)
{
	u32 MacAddr;
	u8 *Aptr = (u8 *) AddressPtr;

        /* Index ranges 1 to 4, for offset calculation is 0 to 3. */
        Index--;

	/* Set the MAC bits [31:0] in BOT */
	MacAddr = Aptr[0];
	MacAddr |= Aptr[1] << 8;
	MacAddr |= Aptr[2] << 16;
	MacAddr |= Aptr[3] << 24;
	out32(dev->iobase,
			   (XEMACPSS_LADDR1L_OFFSET + Index * 8), MacAddr);

	/* There are reserved bits in TOP so don't affect them */
	MacAddr = in32(dev->iobase,
				    (XEMACPSS_LADDR1H_OFFSET + (Index * 8)));

	MacAddr &= ~XEMACPSS_LADDR_MACH_MASK;

	/* Set MAC bits [47:32] in TOP */
	MacAddr |= Aptr[4];
	MacAddr |= Aptr[5] << 8;

	out32(dev->iobase,
			   (XEMACPSS_LADDR1H_OFFSET + (Index * 8)), MacAddr);

	/* Set the ID bits in MATCHx register - settypeidcheck */
	out32(dev->iobase,
			   (XEMACPSS_MATCH1_OFFSET + (Index * 4)), 0);
}
#endif

int gem_init(struct eth_device *dev, bd_t * bis)
{
	int tmp;
	int i;
#if MAC
	char EmacPss_zero_MAC[6] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
#endif
	struct gemac_priv *priv = dev->priv;

	if (priv->initialized)
		return 1;

	/* Disable all interrupts */
	out32(dev->iobase, XEMACPSS_IDR_OFFSET, 0xFFFFFFFF);

	/* Disable the receiver & transmitter */
	out32(dev->iobase, XEMACPSS_NWCTRL_OFFSET, 0);
	out32(dev->iobase, XEMACPSS_TXSR_OFFSET, 0);
	out32(dev->iobase, XEMACPSS_RXSR_OFFSET, 0);
	out32(dev->iobase, XEMACPSS_PHYMNTNC_OFFSET, 0);

#if MAC
	/* Clear the Hash registers for the mac address pointed by AddressPtr */
	out32(dev->iobase, XEMACPSS_HASHL_OFFSET, 0x0);
	/* write bits [63:32] in TOP */
	out32(dev->iobase, XEMACPSS_HASHH_OFFSET, 0x0);

	for (i = 1; i < 5; i++) {
		SetMacAddress(dev, EmacPss_zero_MAC, i);
	}

	/* clear all counters */
	for (i = 0; i < (XEMACPSS_LAST_OFFSET - XEMACPSS_OCTTXL_OFFSET) / 4;
	     i++) {
		in32(dev->iobase, XEMACPSS_OCTTXL_OFFSET + i * 4);
	}
#endif

	/* Setup RxBD space */
	memset(&(priv->rx_bd), 0, sizeof(priv->rx_bd));
	/* Create the RxBD ring */
	memset(&(priv->rxbuffers), 0, sizeof(priv->rxbuffers));

	for (i = 0; i < RX_BUF; i++) {
		priv->rx_bd[i].status = 0xF0000000;
		priv->rx_bd[i].addr = (u32)((char *) &(priv->rxbuffers) +
							(i * PKTSIZE_ALIGN));
	}
	/* WRAP bit to last BD */
	priv->rx_bd[--i].addr |= XEMACPSS_RXBUF_WRAP_MASK;
	/* Write RxBDs to IP */
	out32(dev->iobase,XEMACPSS_RXQBASE_OFFSET, (u32) &(priv->rx_bd));


	/* MAC Setup */
	/*
	 *  Following is the setup for Network Configuration register.
	 *  Bit 0:  Set for 100 Mbps operation.
	 *  Bit 1:  Set for Full Duplex mode.
	 *  Bit 4:  Set to allow Copy all frames.
	 *  Bit 17: Set for FCS removal.
	 *  Bits 20-18: Set with value binary 010 to divide pclk by 32
	 *              (pclk up to 80 MHz)
	 */
/*	out32(dev->iobase,   XEMACPSS_NWCFG_OFFSET,  XEMACPSS_NWCFG_100_MASK |
		XEMACPSS_NWCFG_FDEN_MASK | XEMACPSS_NWCFG_UCASTHASHEN_MASK); */

	out32(dev->iobase, XEMACPSS_NWCFG_OFFSET, 0x000A0013);

	/*
	 * Following is the setup for DMA Configuration register.
	 * Bits 4-0: To set AHB fixed burst length for DMA data operations ->
	 *  Set with binary 00100 to use INCR4 AHB bursts.
	 * Bits 9-8: Receiver packet buffer memory size ->
	 *  Set with binary 11 to Use full configured addressable space (8 Kb)
	 * Bit 10  : Transmitter packet buffer memory size ->
	 *  Set with binary 1 to Use full configured addressable space (4 Kb)
	 * Bits 23-16 : DMA receive buffer size in AHB system memory ->
	 *  Set with binary 00011000 to use 1536 byte(1*max length frame/buffer)
	 */
/*	out32(dev->iobase, XEMACPSS_DMACR_OFFSET,
		((((PKTSIZE_ALIGN / XEMACPSS_RX_BUF_UNIT) +
		((PKTSIZE_ALIGN % XEMACPSS_RX_BUF_UNIT) ? 1 : 0)) <<
			XEMACPSS_DMACR_RXBUF_SHIFT) &
		XEMACPSS_DMACR_RXBUF_MASK) | XEMACPSS_DMACR_RXSIZE_MASK |
		XEMACPSS_DMACR_TXSIZE_MASK); */

	out32(dev->iobase, XEMACPSS_DMACR_OFFSET, 0x00180704);


	/*
	 * Following is the setup for Network Control register.
	 * Bit 2:  Set to enable Receive operation.
	 * Bit 3:  Set to enable Transmitt operation.
	 * Bit 4:  Set to enable MDIO operation.
	 */
	tmp = in32(dev->iobase, XEMACPSS_NWCTRL_OFFSET);
	/* MDIO, Rx and Tx enable */
	tmp |= XEMACPSS_NWCTRL_MDEN_MASK | XEMACPSS_NWCTRL_RXEN_MASK |
	    XEMACPSS_NWCTRL_TXEN_MASK;
	out32(dev->iobase, XEMACPSS_NWCTRL_OFFSET, tmp);


	/* PHY Setup */
	/* "add delay to RGMII rx interface" */
	phy_wr(dev, 20, 0xc93);

	/* link speed advertisement for autonegotiation */
	tmp = phy_rd(dev, 4);
	tmp |= 0xd80;		/* enable 100Mbps */
	tmp &= ~0x60;		/* disable 10 Mbps */
	phy_wr(dev, 4, tmp);

	/* *disable* gigabit advertisement */
	tmp = phy_rd(dev, 9);
	tmp &= ~0x0300;
	phy_wr(dev, 9, tmp);

	/* enable autonegotiation, set 100Mbps, full-duplex, restart aneg */
	tmp = phy_rd(dev, 0);
	phy_wr(dev, 0, 0x3300 | (tmp & 0x1F));

	phy_rst(dev);

	puts("\nWaiting for PHY to complete autonegotiation.");
	tmp = 0;
	while (!(phy_rd(dev, 1) & (1 << 5))) {
		if ((tmp % 50) == 0) {
			putc('.');
		}
		tmp++;
	}
	puts("\nPHY claims autonegotiation complete...\n");

	puts("GEM link speed is 100Mbps\n");

	priv->initialized = 1;
	return 0;
}

int gem_send (struct eth_device *dev, volatile void *ptr, int len)
{
	volatile int status;
	struct gemac_priv *priv = dev->priv;

	if (!priv->initialized) {
		puts("Error GMAC not initialized");
		return -1;
	}

	/* setup BD */
	out32(dev->iobase, XEMACPSS_TXQBASE_OFFSET, (u32)&(priv->tx_bd));

	/* Setup Tx BD */
	memset((void *) &(priv->tx_bd), 0, sizeof(emac_bd));

	priv->tx_bd.addr = (u32)ptr;
	priv->tx_bd.status = len | XEMACPSS_TXBUF_LAST_MASK |
						XEMACPSS_TXBUF_WRAP_MASK;

	/* Start transmit */
	u32 val = in32(dev->iobase, XEMACPSS_NWCTRL_OFFSET) |
						XEMACPSS_NWCTRL_STARTTX_MASK;
        out32(dev->iobase, XEMACPSS_NWCTRL_OFFSET, val);

	/* Read the stat register to know if the packet has been transmitted */
	status =  in32(dev->iobase, XEMACPSS_TXSR_OFFSET);
	if (status &
	    (XEMACPSS_TXSR_HRESPNOK_MASK | XEMACPSS_TXSR_URUN_MASK |
	     XEMACPSS_TXSR_BUFEXH_MASK )) {
		printf("Something has gone wrong here!? Status is 0x%x.\n",
		       status);
	}

	/* Clear Tx status register before leaving . */
	out32(dev->iobase, XEMACPSS_TXSR_OFFSET, status);
	return 0;
}

/* Do not check frame_recd flag in rx_status register 0x20 - just poll BD */
int gem_recv(struct eth_device *dev)
{
	int frame_len;
	struct gemac_priv *priv = dev->priv;

	if (!(priv->rx_bd[priv->rxbd_current].addr & XEMACPSS_RXBUF_NEW_MASK))
		return 0;

	if (!(priv->rx_bd[priv->rxbd_current].status &
			(XEMACPSS_RXBUF_SOF_MASK | XEMACPSS_RXBUF_EOF_MASK))) {
		printf("GEM: SOF or EOF not set for last buffer received!\n");
		return 0;
	}

	frame_len = priv->rx_bd[priv->rxbd_current].status &
							XEMACPSS_RXBUF_LEN_MASK;
	if (frame_len) {
		NetReceive((u8 *) (priv->rx_bd[priv->rxbd_current].addr &
					XEMACPSS_RXBUF_ADD_MASK), frame_len);

		if (priv->rx_bd[priv->rxbd_current].status &
							XEMACPSS_RXBUF_SOF_MASK)
			priv->rx_first_buf = priv->rxbd_current;
		else {
			priv->rx_bd[priv->rxbd_current].addr &=
						~XEMACPSS_RXBUF_NEW_MASK;
			priv->rx_bd[priv->rxbd_current].status = 0xF0000000;
		}

		if (priv->rx_bd[priv->rxbd_current].status &
						XEMACPSS_RXBUF_EOF_MASK) {
			priv->rx_bd[priv->rx_first_buf].addr &=
						~XEMACPSS_RXBUF_NEW_MASK;
			priv->rx_bd[priv->rx_first_buf].status = 0xF0000000;
		}

		if ((++priv->rxbd_current) >= RX_BUF)
			priv->rxbd_current = 0;

		return frame_len;
	}

	return 0;
}

void gem_halt(struct eth_device *dev)
{
	return;
}

int xilinx_gem_initialize (bd_t *bis, int base_addr)
{
	struct eth_device *dev;

	dev = calloc(1, sizeof(*dev));
	if (dev == NULL)
		return -1;

	dev->priv = calloc(1, sizeof(struct gemac_priv));
	if (dev->priv == NULL) {
		free(dev);
		return -1;
	}


	sprintf(dev->name, "XGem.%x", base_addr);

	dev->iobase = base_addr;

	dev->init = gem_init;
	dev->halt = gem_halt;
	dev->send = gem_send;
	dev->recv = gem_recv;

	eth_register(dev);

	return 1;
}
