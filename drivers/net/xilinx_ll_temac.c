/*
 *
 * Xilinx xps_ll_temac ethernet driver for u-boot
 *
 * Author: Yoshio Kashiwagi kashiwagi@co-nss.co.jp
 *
 * Copyright (C) 2008 Nissin Systems Co.,Ltd.
 * March 2008 created
 *
 * Copyright (C) 2008 - 2009 Michal Simek <monstr@monstr.eu>
 * June 2008 Microblaze optimalization, FIFO mode support
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 */

#include <config.h>
#include <common.h>
#include <net.h>
#include <malloc.h>
#include <asm/processor.h>
#include <asm/io.h>

#ifdef XILINX_LLTEMAC_FIFO_BASEADDR
# define FIFO_MODE	1
#elif XILINX_LLTEMAC_SDMA_CTRL_BASEADDR
# define SDMA_MODE	1
#else
# error Xilinx LL Temac: Unsupported mode - Please setup SDMA or FIFO mode
#endif

#undef ETH_HALTING

#ifdef SDMA_MODE
/* XPS_LL_TEMAC SDMA registers definition */
# define TX_NXTDESC_PTR		(((struct ll_priv *)(dev->priv))->sdma + 0x00)
# define TX_CURBUF_ADDR		(((struct ll_priv *)(dev->priv))->sdma + 0x04)
# define TX_CURBUF_LENGTH	(((struct ll_priv *)(dev->priv))->sdma + 0x08)
# define TX_CURDESC_PTR		(((struct ll_priv *)(dev->priv))->sdma + 0x0c)
# define TX_TAILDESC_PTR	(((struct ll_priv *)(dev->priv))->sdma + 0x10)
# define TX_CHNL_CTRL		(((struct ll_priv *)(dev->priv))->sdma + 0x14)
# define TX_IRQ_REG		(((struct ll_priv *)(dev->priv))->sdma + 0x18)
# define TX_CHNL_STS		(((struct ll_priv *)(dev->priv))->sdma + 0x1c)

# define RX_NXTDESC_PTR		(((struct ll_priv *)(dev->priv))->sdma + 0x20)
# define RX_CURBUF_ADDR		(((struct ll_priv *)(dev->priv))->sdma + 0x24)
# define RX_CURBUF_LENGTH	(((struct ll_priv *)(dev->priv))->sdma + 0x28)
# define RX_CURDESC_PTR		(((struct ll_priv *)(dev->priv))->sdma + 0x2c)
# define RX_TAILDESC_PTR	(((struct ll_priv *)(dev->priv))->sdma + 0x30)
# define RX_CHNL_CTRL		(((struct ll_priv *)(dev->priv))->sdma + 0x34)
# define RX_IRQ_REG		(((struct ll_priv *)(dev->priv))->sdma + 0x38)
# define RX_CHNL_STS		(((struct ll_priv *)(dev->priv))->sdma + 0x3c)

# define DMA_CONTROL_REG	(((struct ll_priv *)(dev->priv))->sdma + 0x40)
#endif

/* XPS_LL_TEMAC direct registers definition */
#define TEMAC_RAF0		(dev->iobase + 0x00)
#define TEMAC_TPF0		(dev->iobase + 0x04)
#define TEMAC_IFGP0		(dev->iobase + 0x08)
#define TEMAC_IS0		(dev->iobase + 0x0c)
#define TEMAC_IP0		(dev->iobase + 0x10)
#define TEMAC_IE0		(dev->iobase + 0x14)

#define TEMAC_MSW0		(dev->iobase + 0x20)
#define TEMAC_LSW0		(dev->iobase + 0x24)
#define TEMAC_CTL0		(dev->iobase + 0x28)
#define TEMAC_RDY0		(dev->iobase + 0x2c)

#define XTE_RSE_MIIM_RR_MASK	0x0002
#define XTE_RSE_MIIM_WR_MASK	0x0004
#define XTE_RSE_CFG_RR_MASK	0x0020
#define XTE_RSE_CFG_WR_MASK	0x0040

/* XPS_LL_TEMAC indirect registers offset definition */

#define RCW0	0x200
#define RCW1	0x240
#define TC	0x280
#define FCC	0x2c0
#define EMMC	0x300
#define PHYC	0x320
#define MC	0x340
#define UAW0	0x380
#define UAW1	0x384
#define MAW0	0x388
#define MAW1	0x38c
#define AFM	0x390
#define TIS	0x3a0
#define TIE	0x3a4
#define MIIMWD	0x3b0
#define MIIMAI	0x3b4

#define CNTLREG_WRITE_ENABLE_MASK	0x8000
#define CNTLREG_EMAC1SEL_MASK		0x0400
#define CNTLREG_ADDRESSCODE_MASK	0x03ff

#define MDIO_ENABLE_MASK	0x40
#define MDIO_CLOCK_DIV_MASK	0x3F
#define MDIO_CLOCK_DIV_100MHz	0x28

#define ETHER_MTU		1520

#ifdef SDMA_MODE
/* CDMAC descriptor status bit definitions */
# define BDSTAT_ERROR_MASK		0x80
# define BDSTAT_INT_ON_END_MASK		0x40
# define BDSTAT_STOP_ON_END_MASK	0x20
# define BDSTAT_COMPLETED_MASK		0x10
# define BDSTAT_SOP_MASK		0x08
# define BDSTAT_EOP_MASK		0x04
# define BDSTAT_CHANBUSY_MASK		0x02
# define BDSTAT_CHANRESET_MASK		0x01

/* SDMA Buffer Descriptor */

typedef struct cdmac_bd_t {
	struct cdmac_bd_t *next_p;
	unsigned char *phys_buf_p;
	unsigned long buf_len;
	unsigned char stat;
	unsigned char app1_1;
	unsigned short app1_2;
	unsigned long app2;
	unsigned long app3;
	unsigned long app4;
	unsigned long app5;
} cdmac_bd __attribute((aligned(32))) ;

static cdmac_bd	tx_bd;
static cdmac_bd	rx_bd;
#endif

#ifdef FIFO_MODE
typedef struct ll_fifo_s {
	int isr; /* Interrupt Status Register 0x0 */
	int ier; /* Interrupt Enable Register 0x4 */
	int tdfr; /* Transmit data FIFO reset 0x8 */
	int tdfv; /* Transmit data FIFO Vacancy 0xC */
	int tdfd; /* Transmit data FIFO 32bit wide data write port 0x10 */
	int tlf; /* Write Transmit Length FIFO 0x14 */
	int rdfr; /* Read Receive data FIFO reset 0x18 */
	int rdfo; /* Receive data FIFO Occupancy 0x1C */
	int rdfd; /* Read Receive data FIFO 32bit wide data read port 0x20 */
	int rlf; /* Read Receive Length FIFO 0x24 */
	int llr; /* Read LocalLink reset 0x28 */
} ll_fifo_s;

ll_fifo_s *ll_fifo = (ll_fifo_s *) (XILINX_LLTEMAC_FIFO_BASEADDR);
#endif

#ifdef SDMA_MODE
static unsigned char tx_buffer[ETHER_MTU] __attribute((aligned(32)));
#endif
static unsigned char rx_buffer[ETHER_MTU] __attribute((aligned(32)));

struct ll_priv {
	unsigned int sdma;
};

#ifdef DEBUG
/* undirect hostif write to ll_temac */
static void xps_ll_temac_hostif_set(struct eth_device *dev, int emac,
			int phy_addr, int reg_addr, int phy_data)
{
	out_be32((u32 *)TEMAC_LSW0, phy_data);
	out_be32((u32 *)TEMAC_CTL0, CNTLREG_WRITE_ENABLE_MASK | MIIMWD);
	out_be32((u32 *)TEMAC_LSW0, (phy_addr << 5) | (reg_addr));
	out_be32((u32 *)TEMAC_CTL0, \
			CNTLREG_WRITE_ENABLE_MASK | MIIMAI | (emac << 10));
	while(! (in_be32((u32 *)TEMAC_RDY0) & XTE_RSE_MIIM_WR_MASK));
}
#endif

/* undirect hostif read from ll_temac */
static unsigned int xps_ll_temac_hostif_get(struct eth_device *dev,
			int emac, int phy_addr, int reg_addr)
{
	out_be32((u32 *)TEMAC_LSW0, (phy_addr << 5) | (reg_addr));
	out_be32((u32 *)TEMAC_CTL0, MIIMAI | (emac << 10));
	while(! (in_be32((u32 *)TEMAC_RDY0) & XTE_RSE_MIIM_RR_MASK));
	return in_be32((u32 *)TEMAC_LSW0);
}

/* undirect write to ll_temac */
static void xps_ll_temac_indirect_set(struct eth_device *dev,
				int emac, int reg_offset, int reg_data)
{
	out_be32((u32 *)TEMAC_LSW0, reg_data);
	out_be32((u32 *)TEMAC_CTL0, \
			CNTLREG_WRITE_ENABLE_MASK | (emac << 10) | reg_offset);
	while(! (in_be32((u32 *)TEMAC_RDY0) & XTE_RSE_CFG_WR_MASK));
}

#if DEBUG
/* undirect read from ll_temac */
static int xps_ll_temac_indirect_get(struct eth_device *dev,
			int emac, int reg_offset)
{
	out_be32((u32 *)TEMAC_CTL0, (emac << 10) | reg_offset);
	while(! (in_be32((u32 *)TEMAC_RDY0) & XTE_RSE_CFG_RR_MASK));
	return in_be32((u32 *)TEMAC_LSW0);
}
#endif

#ifdef DEBUG
/* read from phy */
static void read_phy_reg (struct eth_device *dev, int phy_addr)
{
	int j, result;
	debug ("phy%d ",phy_addr);
	for ( j = 0; j < 32; j++) {
		result = xps_ll_temac_hostif_get(dev, 0, phy_addr, j);
		debug ("%d: 0x%x ", j, result);
	}
	debug ("\n");
}
#endif

static int phy_addr = -1;
static int link = 0;

/* setting ll_temac and phy to proper setting */
static int xps_ll_temac_phy_ctrl(struct eth_device *dev)
{
	int i;
	unsigned int result;
	unsigned retries = 10;

	if(link == 1)
		return 1; /* link is setup */

	/* wait for link up */
	while (retries-- &&
		((xps_ll_temac_hostif_get(dev, 0, phy_addr, 1) & 0x24) == 0x24))
		;

	if(phy_addr == -1) {
		for(i = 31; i >= 0; i--) {
			result = xps_ll_temac_hostif_get(dev, 0, i, 1);
			if((result & 0x0ffff) != 0x0ffff) {
				debug ("phy %x result %x\n", i, result);
				phy_addr = i;
				break;
			}
		}
	}

	/* get PHY id */
	i = (xps_ll_temac_hostif_get(dev, 0, phy_addr, 2) << 16) | \
		xps_ll_temac_hostif_get(dev, 0, phy_addr, 3);
	debug ("LL_TEMAC: Phy ID 0x%x\n", i);

#ifdef DEBUG
	xps_ll_temac_hostif_set(dev, 0, 0, 0, 0x8000); /* phy reset */
#endif
	/* FIXME this part will be replaced by PHY lib */
	/* s3e boards */
	if (i == 0x7c0a3) {
		/* 100BASE-T/FD */
		xps_ll_temac_indirect_set(dev, 0, EMMC, 0x40000000);
		link = 1;
		return 1;
	}

	/* Marwell 88e1111 id - ml50x */
	if (i == 0x1410cc2) {
		result = xps_ll_temac_hostif_get(dev, 0, phy_addr, 5);
		if((result & 0x8000) == 0x8000) {
			xps_ll_temac_indirect_set(dev, 0, EMMC, 0x80000000);
			printf("1000BASE-T/FD\n");
			link = 1;
		} else if((result & 0x4000) == 0x4000) {
			xps_ll_temac_indirect_set(dev, 0, EMMC, 0x40000000);
			printf("100BASE-T/FD\n");
			link = 1;
		} else {
			printf("Unsupported mode\n");
			link = 0;
		}
		return 1;
	}
	return 0;
}

#ifdef SDMA_MODE
/* bd init */
static void xps_ll_temac_bd_init(struct eth_device *dev)
{
	memset((void *)&tx_bd, 0, sizeof(cdmac_bd));
	memset((void *)&rx_bd, 0, sizeof(cdmac_bd));

	rx_bd.phys_buf_p = &rx_buffer[0];

	rx_bd.next_p = &rx_bd;
	rx_bd.buf_len = ETHER_MTU;
	flush_cache((u32)&rx_bd, sizeof(cdmac_bd));

	out_be32((u32 *)RX_CURDESC_PTR, (u32)&rx_bd);
	out_be32((u32 *)RX_TAILDESC_PTR, (u32)&rx_bd);
	out_be32((u32 *)RX_NXTDESC_PTR, (u32)&rx_bd); /* setup first fd */

	tx_bd.phys_buf_p = &tx_buffer[0];
	tx_bd.next_p = &tx_bd;

	flush_cache((u32)&tx_bd, sizeof(cdmac_bd));
	out_be32((u32 *)TX_CURDESC_PTR, (u32)&tx_bd);
}

static int xps_ll_temac_send_sdma(struct eth_device *dev,
				unsigned char *buffer, int length)
{
	if( xps_ll_temac_phy_ctrl(dev) == 0)
		return 0;

	memcpy (tx_buffer, buffer, length);
	flush_cache ((u32)tx_buffer, length);

	tx_bd.stat = BDSTAT_SOP_MASK | BDSTAT_EOP_MASK | \
			BDSTAT_STOP_ON_END_MASK;
	tx_bd.buf_len = length;
	flush_cache ((u32)&tx_bd, sizeof(cdmac_bd));

	out_be32((u32 *)TX_CURDESC_PTR, (u32)&tx_bd);
	out_be32((u32 *)TX_TAILDESC_PTR, (u32)&tx_bd); /* DMA start */

	do {
		flush_cache ((u32)&tx_bd, sizeof(cdmac_bd));
	} while(!(((volatile int)tx_bd.stat) & BDSTAT_COMPLETED_MASK));

	return length;
}

static int xps_ll_temac_recv_sdma(struct eth_device *dev)
{
	int length;

	flush_cache ((u32)&rx_bd, sizeof(cdmac_bd));

	if(!(rx_bd.stat & BDSTAT_COMPLETED_MASK)) {
		return 0;
	}

	length = rx_bd.app5 & 0x3FFF;
	flush_cache ((u32)rx_bd.phys_buf_p, length);

	rx_bd.buf_len = ETHER_MTU;
	rx_bd.stat = 0;
	rx_bd.app5 = 0;

	flush_cache ((u32)&rx_bd, sizeof(cdmac_bd));
	out_be32((u32 *)RX_TAILDESC_PTR, (u32)&rx_bd);

	if(length > 0) {
		NetReceive(rx_bd.phys_buf_p, length);
	}

	return length;
}
#endif

#ifdef FIFO_MODE
#ifdef DEBUG
static void debugll(int count)
{
	printf ("%d fifo isr 0x%08x, fifo_ier 0x%08x, fifo_rdfr 0x%08x, "
		"fifo_rdfo 0x%08x fifo_rlr 0x%08x\n",count, ll_fifo->isr, \
		ll_fifo->ier, ll_fifo->rdfr, ll_fifo->rdfo, ll_fifo->rlf);
}
#endif

static int xps_ll_temac_send_fifo(unsigned char *buffer, int length)
{
	u32 *buf = (u32 *)buffer;
	u32 len, i, val;

	len = (length / 4) + 1;

	for (i = 0; i < len; i++) {
		val = *buf++;
		ll_fifo->tdfd = val;
	}

	ll_fifo->tlf = length;

	return length;
}

static int xps_ll_temac_recv_fifo(void)
{
	u32 len, len2, i, val;
	u32 *buf = (u32 *)&rx_buffer;

	if (ll_fifo->isr & 0x04000000 ) {
		ll_fifo->isr = 0xffffffff; /* reset isr */
	
		/* while (ll_fifo->isr); */
		len = ll_fifo->rlf & 0x7FF;
		len2 = (len / 4) + 1;
	
		for (i = 0; i < len2; i++) {
			val = ll_fifo->rdfd;
			*buf++ = val ;
		}
	
		/* debugll(1); */
		NetReceive ((uchar *)&rx_buffer, len);
	}
	return 0;
}
#endif

/* setup mac addr */
static int xps_ll_temac_addr_setup(struct eth_device *dev)
{
	int val;

	/* set up unicast MAC address filter */
	val = ((dev->enetaddr[3] << 24) | (dev->enetaddr[2] << 16) |
		(dev->enetaddr[1] << 8) | (dev->enetaddr[0] ));
	xps_ll_temac_indirect_set(dev, 0, UAW0, val);
	val = (dev->enetaddr[5] << 8) | dev->enetaddr[4] ;
	xps_ll_temac_indirect_set(dev, 0, UAW1, val);

	return 0;
}

static int xps_ll_temac_init(struct eth_device *dev, bd_t *bis)
{
#ifdef SDMA_MODE
	xps_ll_temac_bd_init(dev);
#endif
#ifdef FIFO_MODE
	ll_fifo->tdfr = 0x000000a5; /* set fifo length */
	ll_fifo->rdfr = 0x000000a5;

	/* ll_fifo->isr = 0x0; */
	/* ll_fifo->ier = 0x0; */
#endif
	xps_ll_temac_indirect_set(dev, 0, MC,
				MDIO_ENABLE_MASK | MDIO_CLOCK_DIV_100MHz);

	xps_ll_temac_addr_setup(dev);
	/* Promiscuous mode disable */
	xps_ll_temac_indirect_set(dev, 0, AFM, 0x00000000);
	/* Enable Receiver */
	xps_ll_temac_indirect_set(dev, 0, RCW1, 0x10000000);
	/* Enable Transmitter */
	xps_ll_temac_indirect_set(dev, 0, TC, 0x10000000);
	return 0;
}


static void xps_ll_temac_halt(struct eth_device *dev)
{
#ifdef ETH_HALTING
	/* Disable Receiver */
	xps_ll_temac_indirect_set(dev, 0, RCW1, 0x00000000);
	/* Disable Transmitter */
	xps_ll_temac_indirect_set(dev, 0, TC, 0x00000000);

#ifdef SDMA_MODE
	out_be32((u32 *)DMA_CONTROL_REG, 0x00000001);
	while(in_be32((u32 *)DMA_CONTROL_REG) & 1);
#endif
#ifdef FIFO_MODE
	/* reset fifos */
#endif
#endif
}

/* halt device */
static void ll_temac_halt(struct eth_device *dev)
{
	link = 0;
	xps_ll_temac_halt(dev);
}

static int ll_temac_init(struct eth_device *dev, bd_t *bis)
{
	static int first = 1;
#if DEBUG
	int i;
#endif
	if(!first)
		return 0;
	first = 0;

	xps_ll_temac_init(dev, bis);

	printf("%s: Xilinx XPS LocalLink Tri-Mode Ether MAC #%d at 0x%08X.\n",
		dev->name, 0, dev->iobase);

#if DEBUG
	for(i = 0; i < 32; i++) {
		read_phy_reg(dev, i);
	}
#endif
	xps_ll_temac_phy_ctrl(dev);
	return 1;
}


static int ll_temac_send(struct eth_device *dev, volatile void *packet,
		int length)
{
#ifdef SDMA_MODE
	return xps_ll_temac_send_sdma(dev, (unsigned char *)packet, length);
#endif
#ifdef FIFO_MODE
	return xps_ll_temac_send_fifo((unsigned char *)packet, length);
#endif
}

static int ll_temac_recv(struct eth_device *dev)
{
#ifdef SDMA_MODE
	return xps_ll_temac_recv_sdma(dev);
#endif
#ifdef FIFO_MODE
	return xps_ll_temac_recv_fifo();
#endif
}

int xilinx_ll_temac_initialize (bd_t *bis)
{
	struct eth_device *dev;

	dev = calloc(1, sizeof(*dev));
	if (dev == NULL)
		hang();

	dev->priv = (struct ll_priv *)
			calloc(1, sizeof(struct ll_priv));

	if (dev->priv == NULL)
		hang();

	sprintf(dev->name, "Xilinx LL TEMAC");

	dev->iobase = XILINX_LLTEMAC_BASEADDR;
#ifdef SDMA_MODE
	((struct ll_priv *)(dev->priv))->sdma =
					XILINX_LLTEMAC_SDMA_CTRL_BASEADDR;
#endif

	dev->init = ll_temac_init;
	dev->halt = ll_temac_halt;
	dev->send = ll_temac_send;
	dev->recv = ll_temac_recv;

	eth_register(dev);

	return 0;
}
