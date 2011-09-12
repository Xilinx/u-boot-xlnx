/*
 * Xilinx xps_ll_temac ethernet driver for u-boot
 *
 * Author: Yoshio Kashiwagi kashiwagi@co-nss.co.jp
 *
 * Copyright (C) 2008 Nissin Systems Co.,Ltd.
 * March 2008 created
 *
 * Copyright (C) 2008 - 2010 Michal Simek <monstr@monstr.eu>
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

#undef ETH_HALTING

#define ETHER_MTU		1520

#define XTE_EMMC_LINKSPEED_MASK	0xC0000000 /* Link speed */
/* XTE_EMCFG_LINKSPD_MASK */
#define XTE_EMMC_LINKSPD_10	0x00000000 /* for 10 Mbit */
#define XTE_EMMC_LINKSPD_100	0x40000000 /* for 100 Mbit */
#define XTE_EMMC_LINKSPD_1000	0x80000000 /* forr 1000 Mbit */

/* XPS_LL_TEMAC direct registers definition */
// #define TEMAC_RAF0		0x00
// #define TEMAC_TPF0		0x04
// #define TEMAC_IFGP0		0x08
// #define TEMAC_IS0		0x0c
// #define TEMAC_IP0		0x10
// #define TEMAC_IE0		0x14

// #define TEMAC_MSW0		0x20
#define TEMAC_LSW0		0x24
#define TEMAC_CTL0		0x28
#define TEMAC_RDY0		0x2c

#define XTE_RSE_MIIM_RR_MASK	0x0002
#define XTE_RSE_MIIM_WR_MASK	0x0004
#define XTE_RSE_CFG_RR_MASK	0x0020
#define XTE_RSE_CFG_WR_MASK	0x0040

/* XPS_LL_TEMAC indirect registers offset definition */

// #define RCW0	0x200
#define RCW1	0x240
#define TC	0x280
// #define FCC	0x2c0
#define EMMC	0x300
// #define PHYC	0x320
#define MC	0x340
#define UAW0	0x380
#define UAW1	0x384
// #define MAW0	0x388
// #define MAW1	0x38c
#define AFM	0x390
// #define TIS	0x3a0
// #define TIE	0x3a4
#define MIIMWD	0x3b0
#define MIIMAI	0x3b4

#define CNTLREG_WRITE_ENABLE_MASK	0x8000
// #define CNTLREG_EMAC1SEL_MASK		0x0400
// #define CNTLREG_ADDRESSCODE_MASK	0x03ff

#define MDIO_ENABLE_MASK	0x40
// #define MDIO_CLOCK_DIV_MASK	0x3F
#define MDIO_CLOCK_DIV_100MHz	0x28

/* XPS_LL_TEMAC SDMA registers definition */
//# define TX_NXTDESC_PTR	0x00
//# define TX_CURBUF_ADDR	0x01
//# define TX_CURBUF_LENGTH	0x02
# define TX_CURDESC_PTR		0x03
# define TX_TAILDESC_PTR	0x04
# define TX_CHNL_CTRL		0x05
# define TX_IRQ_REG		0x06
# define TX_CHNL_STS		0x07

# define RX_NXTDESC_PTR		0x08
//# define RX_CURBUF_ADDR		0x09
//# define RX_CURBUF_LENGTH	0x0a
# define RX_CURDESC_PTR		0x0b
# define RX_TAILDESC_PTR	0x0c
# define RX_CHNL_CTRL		0x0d
# define RX_IRQ_REG		0x0e
# define RX_CHNL_STS		0x0f

# define DMA_CONTROL_REG	0x10

/* CDMAC descriptor status bit definitions */
// # define BDSTAT_ERROR_MASK		0x80
// # define BDSTAT_INT_ON_END_MASK		0x40
# define BDSTAT_STOP_ON_END_MASK	0x20
# define BDSTAT_COMPLETED_MASK		0x10
# define BDSTAT_SOP_MASK		0x08
# define BDSTAT_EOP_MASK		0x04
// # define BDSTAT_CHANBUSY_MASK		0x02
// # define BDSTAT_CHANRESET_MASK		0x01

# define CHNL_STS_ERROR_MASK		0x80

#define XLLDMA_CR_IRQ_ALL_EN_MASK       0x00000087 /**< All interrupt enable
                                                        bits */

// #define XLLDMA_IRQ_ALL_ERR_MASK          0x0000001C /**< All error interrupt */
#define XLLDMA_IRQ_ALL_MASK              0x0000001F /**< All interrupt bits */

// #define XLLDMA_DMACR_TX_PAUSE_MASK             0x20000000 /**< Pause TX channel */
// #define XLLDMA_DMACR_RX_PAUSE_MASK             0x10000000 /**< Pause RX channel */
// #define XLLDMA_DMACR_PLB_ERR_DIS_MASK          0x00000020 /**< Disable PLB error detection */
#define XLLDMA_DMACR_RX_OVERFLOW_ERR_DIS_MASK  0x00000010 /**< Disable error
                                                               when 2 or 4 bit
                                                               coalesce counter
                                                               overflows */
#define XLLDMA_DMACR_TX_OVERFLOW_ERR_DIS_MASK  0x00000008 /**< Disable error
                                                               when 2 or 4 bit
                                                               coalesce counter
                                                               overflows */
#define XLLDMA_DMACR_TAIL_PTR_EN_MASK          0x00000004 /**< Enable use of
                                                               tail pointer
                                                               register */
// #define XLLDMA_DMACR_EN_ARB_HOLD_MASK          0x00000002 /**< Enable arbitration hold */
// #define XLLDMA_DMACR_SW_RESET_MASK             0x00000001 /**< Assert Software reset for both channels */

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

struct ll_fifo_s {
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
};

static unsigned char tx_buffer[ETHER_MTU] __attribute((aligned(32)));
static unsigned char rx_buffer[ETHER_MTU] __attribute((aligned(32)));

struct ll_priv {
	unsigned int ctrl;
	unsigned int mode;
	int phyaddr;
	unsigned int initialized;
};

static void mtdcr_local(u32 reg, u32 val)
{
#if defined(CONFIG_XILINX_440)
	mtdcr(0x00, reg);
	mtdcr(0x01, val);
#endif
}

static u32 mfdcr_local(u32 reg)
{
	u32 val = 0;
#if defined(CONFIG_XILINX_440)
	mtdcr(0x00, reg);
	val = mfdcr(0x01);
#endif
	return val;
}

static void sdma_out_be32(struct ll_priv *priv, u32 offset, u32 val)
{
	if (priv->mode & 0x2)
		mtdcr_local(priv->ctrl + offset, val);
	else
		out_be32((u32 *)(priv->ctrl + offset * 4), val);
}

static u32 sdma_in_be32(struct ll_priv *priv, u32 offset)
{
	if (priv->mode & 0x2)
		return mfdcr_local(priv->ctrl + offset);

	return in_be32((u32 *)(priv->ctrl + offset * 4));
}

static inline void temac_out_be32(u32 addr, u32 offset, u32 val)
{
	out_be32((u32 *)(addr + offset), val);
}

static inline u32 temac_in_be32(u32 addr, u32 offset)
{
	return in_be32((u32 *)(addr + offset));
}

#ifdef DEBUG
/* undirect hostif write to ll_temac */
static void xps_ll_temac_hostif_set(struct eth_device *dev, int emac,
			int phy_addr, int reg_addr, int phy_data)
{
	temac_out_be32(dev->iobase, TEMAC_LSW0, phy_data);
	temac_out_be32(dev->iobase, TEMAC_CTL0,
					CNTLREG_WRITE_ENABLE_MASK | MIIMWD);
	temac_out_be32(dev->iobase, TEMAC_LSW0, (phy_addr << 5) | (reg_addr));
	temac_out_be32(dev->iobase, TEMAC_CTL0,
			CNTLREG_WRITE_ENABLE_MASK | MIIMAI | (emac << 10));
	while (!(temac_in_be32(dev->iobase, TEMAC_RDY0) & XTE_RSE_MIIM_WR_MASK))
		;
}
#endif

/* undirect hostif read from ll_temac */
static unsigned int xps_ll_temac_hostif_get(struct eth_device *dev,
			int emac, int phy_addr, int reg_addr)
{
	temac_out_be32(dev->iobase, TEMAC_LSW0, (phy_addr << 5) | (reg_addr));
	temac_out_be32(dev->iobase, TEMAC_CTL0, MIIMAI | (emac << 10));
	while (!(temac_in_be32(dev->iobase, TEMAC_RDY0) & XTE_RSE_MIIM_RR_MASK))
		;
	return temac_in_be32(dev->iobase, TEMAC_LSW0);
}

/* undirect write to ll_temac */
static void xps_ll_temac_indirect_set(struct eth_device *dev,
				int emac, int reg_offset, int reg_data)
{
	temac_out_be32(dev->iobase, TEMAC_LSW0, reg_data);
	temac_out_be32(dev->iobase, TEMAC_CTL0,
			CNTLREG_WRITE_ENABLE_MASK | (emac << 10) | reg_offset);
	while (!(temac_in_be32(dev->iobase, TEMAC_RDY0) & XTE_RSE_CFG_WR_MASK))
		;
}

/* undirect read from ll_temac */
static int xps_ll_temac_indirect_get(struct eth_device *dev,
			int emac, int reg_offset)
{
	temac_out_be32(dev->iobase, TEMAC_CTL0, (emac << 10) | reg_offset);
	while (!(temac_in_be32(dev->iobase, TEMAC_RDY0) & XTE_RSE_CFG_RR_MASK))
		;
	return temac_in_be32(dev->iobase, TEMAC_LSW0);
}

#ifdef DEBUG
/* read from phy */
static void read_phy_reg (struct eth_device *dev, int phy_addr)
{
	int j, result;
	debug("phy%d ", phy_addr);
	for (j = 0; j < 32; j++) {
		result = xps_ll_temac_hostif_get(dev, 0, phy_addr, j);
		debug("%d: 0x%x ", j, result);
	}
	debug("\n");
}
#endif

/* setting ll_temac and phy to proper setting */
static int xps_ll_temac_phy_ctrl(struct eth_device *dev)
{
	int i;
	unsigned int result;
	struct ll_priv *priv = dev->priv;
	unsigned retries = 10;
	unsigned int phyreg = 0;

	/* wait for link up */
	puts("Waiting for link ... ");
	retries = 10;
	while (retries-- &&
		((xps_ll_temac_hostif_get(dev, 0, priv->phyaddr, 1) & 0x24) != 0x24))
			;

	/* try out if have ever found the right phy? */
	if (priv->phyaddr == -1) {
		for (i = 31; i >= 0; i--) {
			result = xps_ll_temac_hostif_get(dev, 0, i, 1);
			if ((result & 0x0ffff) != 0x0ffff) {
				debug("phy %x result %x\n", i, result);
				priv->phyaddr = i;
				break;
			}
		}
	}

	/* get PHY id */
	i = (xps_ll_temac_hostif_get(dev, 0, priv->phyaddr, 2) << 16) |
		xps_ll_temac_hostif_get(dev, 0, priv->phyaddr, 3);
	debug("LL_TEMAC: Phy ID 0x%x\n", i);

#ifdef DEBUG
	xps_ll_temac_hostif_set(dev, 0, priv->phyaddr, 0, 0x8000); /* phy reset */
	read_phy_reg(dev, priv->phyaddr);
#endif
	phyreg = xps_ll_temac_indirect_get(dev, 0, EMMC) & (~XTE_EMMC_LINKSPEED_MASK);
	/* FIXME this part will be replaced by PHY lib */
	/* s3e boards */
	if (i == 0x7c0a3) {
		/* 100BASE-T/FD */
		xps_ll_temac_indirect_set(dev, 0, EMMC, (phyreg | XTE_EMMC_LINKSPD_100));
		return 1;
	}

	result = xps_ll_temac_hostif_get(dev, 0, priv->phyaddr, 5);
	if((result & 0x8000) == 0x8000) {
		xps_ll_temac_indirect_set(dev, 0, EMMC, (phyreg | XTE_EMMC_LINKSPD_1000));
		printf("1000BASE-T/FD\n");
	} else if((result & 0x4000) == 0x4000) {
		xps_ll_temac_indirect_set(dev, 0, EMMC, (phyreg | XTE_EMMC_LINKSPD_100));
		printf("100BASE-T/FD\n");
	} else {
		printf("Unsupported mode\n");
		return 0;
	}

	return 1;
}

static inline int xps_ll_temac_dma_error(struct eth_device *dev)
{
	int err;
	struct ll_priv *priv = dev->priv;

	/* Check for TX and RX channel errrors.  */
	err = sdma_in_be32(priv, TX_CHNL_STS) & CHNL_STS_ERROR_MASK;
	err |= sdma_in_be32(priv, RX_CHNL_STS) & CHNL_STS_ERROR_MASK;
	return err;
}

static void xps_ll_temac_reset_dma(struct eth_device *dev)
{
	u32 r;
	struct ll_priv *priv = dev->priv;

	/* Soft reset the DMA.  */
	sdma_out_be32(priv, DMA_CONTROL_REG, 0x00000001);
	while (sdma_in_be32(priv, DMA_CONTROL_REG) & 1)
		;

	/* Now clear the interrupts.  */
	r = sdma_in_be32(priv, TX_CHNL_CTRL);
	r &= ~XLLDMA_CR_IRQ_ALL_EN_MASK;
	sdma_out_be32(priv, TX_CHNL_CTRL, r);

	r = sdma_in_be32(priv, RX_CHNL_CTRL);
	r &= ~XLLDMA_CR_IRQ_ALL_EN_MASK;
	sdma_out_be32(priv, RX_CHNL_CTRL, r);

	/* Now ACK pending IRQs.  */
	sdma_out_be32(priv, TX_IRQ_REG, XLLDMA_IRQ_ALL_MASK);
	sdma_out_be32(priv, RX_IRQ_REG, XLLDMA_IRQ_ALL_MASK);

	/* Set tail-ptr mode, disable errors for both channels.  */
	sdma_out_be32(priv, DMA_CONTROL_REG,
			XLLDMA_DMACR_TAIL_PTR_EN_MASK |
			XLLDMA_DMACR_RX_OVERFLOW_ERR_DIS_MASK |
			XLLDMA_DMACR_TX_OVERFLOW_ERR_DIS_MASK);
}

/* bd init */
static void xps_ll_temac_bd_init(struct eth_device *dev)
{
	struct ll_priv *priv = dev->priv;
	memset((void *)&tx_bd, 0, sizeof(cdmac_bd));
	memset((void *)&rx_bd, 0, sizeof(cdmac_bd));

	rx_bd.phys_buf_p = &rx_buffer[0];

	rx_bd.next_p = &rx_bd;
	rx_bd.buf_len = ETHER_MTU;
	flush_cache((u32)&rx_bd, sizeof(cdmac_bd));
	flush_cache ((u32)rx_bd.phys_buf_p, ETHER_MTU);

	sdma_out_be32(priv, RX_CURDESC_PTR, (u32)&rx_bd);
	sdma_out_be32(priv, RX_TAILDESC_PTR, (u32)&rx_bd);
	sdma_out_be32(priv, RX_NXTDESC_PTR, (u32)&rx_bd); /* setup first fd */

	tx_bd.phys_buf_p = &tx_buffer[0];
	tx_bd.next_p = &tx_bd;

	flush_cache((u32)&tx_bd, sizeof(cdmac_bd));
	sdma_out_be32(priv, TX_CURDESC_PTR, (u32)&tx_bd);
}

static int ll_temac_send_sdma(struct eth_device *dev,
				volatile void *buffer, int length)
{
	struct ll_priv *priv = dev->priv;

	if (xps_ll_temac_dma_error(dev)) {
		xps_ll_temac_reset_dma(dev);
		xps_ll_temac_bd_init(dev);
	}

	memcpy(tx_buffer, (void *)buffer, length);
	flush_cache ((u32)tx_buffer, length);

	tx_bd.stat = BDSTAT_SOP_MASK | BDSTAT_EOP_MASK |
			BDSTAT_STOP_ON_END_MASK;
	tx_bd.buf_len = length;
	flush_cache ((u32)&tx_bd, sizeof(cdmac_bd));

	sdma_out_be32(priv, TX_CURDESC_PTR, (u32)&tx_bd);
	sdma_out_be32(priv, TX_TAILDESC_PTR, (u32)&tx_bd); /* DMA start */

	do {
		flush_cache ((u32)&tx_bd, sizeof(cdmac_bd));
	} while (!(((volatile int)tx_bd.stat) & BDSTAT_COMPLETED_MASK));

	return 0;
}

static int ll_temac_recv_sdma(struct eth_device *dev)
{
	int length;
	struct ll_priv *priv = dev->priv;

	if (xps_ll_temac_dma_error(dev)) {
		xps_ll_temac_reset_dma(dev);
		xps_ll_temac_bd_init(dev);
	}

	flush_cache ((u32)&rx_bd, sizeof(cdmac_bd));

	if (!(rx_bd.stat & BDSTAT_COMPLETED_MASK))
		return 0;

	/* Read out the packet info and start the DMA
	   onto the second buffer to enable the ethernet rx
	   path to run in parallel with sw processing
	   packets.  */
	length = rx_bd.app5 & 0x3FFF;
	if(length > 0) {
		NetReceive(rx_bd.phys_buf_p, length);
	}

	/* flip the buffer and re-enable the DMA.  */
	flush_cache ((u32)rx_bd.phys_buf_p, length);

	rx_bd.buf_len = ETHER_MTU;
	rx_bd.stat = 0;
	rx_bd.app5 = 0;

	flush_cache ((u32)&rx_bd, sizeof(cdmac_bd));
	sdma_out_be32(priv, RX_TAILDESC_PTR, (u32)&rx_bd);

	return length;
}

#ifdef DEBUG
static void debugll(struct eth_device *dev, int count)
{
	struct ll_priv *priv = dev->priv;
	struct ll_fifo_s *ll_fifo = (void *)priv->ctrl;
	printf("%d fifo isr 0x%08x, fifo_ier 0x%08x, fifo_rdfr 0x%08x, "
		"fifo_rdfo 0x%08x fifo_rlr 0x%08x\n", count, ll_fifo->isr,
		ll_fifo->ier, ll_fifo->rdfr, ll_fifo->rdfo, ll_fifo->rlf);
}
#endif

static int ll_temac_send_fifo(struct eth_device *dev,
					volatile void *buffer, int length)
{
	struct ll_priv *priv = dev->priv;
	struct ll_fifo_s *ll_fifo = (void *)priv->ctrl;
	u32 *buf = (u32 *)buffer;
	u32 len, i, val;

	len = (length / 4) + 1;

	for (i = 0; i < len; i++) {
		val = *buf++;
		ll_fifo->tdfd = val;
	}

	ll_fifo->tlf = length;

	return 0;
}

static int ll_temac_recv_fifo(struct eth_device *dev)
{
	struct ll_priv *priv = dev->priv;
	struct ll_fifo_s *ll_fifo = (void *)priv->ctrl;
	u32 len = 0;
	u32 len2, i, val;
	u32 *buf = (u32 *)&rx_buffer;

	if (ll_fifo->isr & 0x04000000) {
		ll_fifo->isr = 0xffffffff; /* reset isr */

		/* while (ll_fifo->isr); */
		len = ll_fifo->rlf & 0x7FF;
		len2 = (len / 4) + 1;

		for (i = 0; i < len2; i++) {
			val = ll_fifo->rdfd;
			*buf++ = val ;
		}

		/* debugll(dev, 1); */
		NetReceive ((uchar *)&rx_buffer, len);
	}
	return len;
}

/* setup mac addr */
static int ll_temac_addr_setup(struct eth_device *dev)
{
	int val;

	/* set up unicast MAC address filter */
	val = ((dev->enetaddr[3] << 24) | (dev->enetaddr[2] << 16) |
		(dev->enetaddr[1] << 8) | (dev->enetaddr[0]));
	xps_ll_temac_indirect_set(dev, 0, UAW0, val);
	val = (dev->enetaddr[5] << 8) | dev->enetaddr[4] ;
	xps_ll_temac_indirect_set(dev, 0, UAW1, val);

	return 0;
}

static int xps_ll_temac_init(struct eth_device *dev, bd_t *bis)
{
	struct ll_priv *priv = dev->priv;
	struct ll_fifo_s *ll_fifo = (void *)priv->ctrl;

	if (priv->mode & 0x1) {
		xps_ll_temac_reset_dma(dev);
		xps_ll_temac_bd_init(dev);
	} else {
		ll_fifo->tdfr = 0x000000a5; /* set fifo length */
		ll_fifo->rdfr = 0x000000a5;
		/* ll_fifo->isr = 0x0; */
		/* ll_fifo->ier = 0x0; */
	}

	xps_ll_temac_indirect_set(dev, 0, MC,
				MDIO_ENABLE_MASK | MDIO_CLOCK_DIV_100MHz);

	/* Promiscuous mode disable */
	xps_ll_temac_indirect_set(dev, 0, AFM, 0x00000000);
	/* Enable Receiver */
	xps_ll_temac_indirect_set(dev, 0, RCW1, 0x10000000);
	/* Enable Transmitter */
	xps_ll_temac_indirect_set(dev, 0, TC, 0x10000000);
	return 0;
}

/* halt device */
static void ll_temac_halt(struct eth_device *dev)
{
#ifdef ETH_HALTING
	struct ll_priv *priv = dev->priv;
	/* Disable Receiver */
	xps_ll_temac_indirect_set(dev, 0, RCW1, 0x00000000);
	/* Disable Transmitter */
	xps_ll_temac_indirect_set(dev, 0, TC, 0x00000000);

	if (priv->mode & 0x1) {
		sdma_out_be32(priv->ctrl, DMA_CONTROL_REG, 0x00000001);
		while (sdma_in_be32(priv->ctrl, DMA_CONTROL_REG) & 1)
			;
	}
	/* reset fifos */
#endif
}

static int ll_temac_init(struct eth_device *dev, bd_t *bis)
{
	struct ll_priv *priv = dev->priv;
#if DEBUG
	int i;
#endif
	if (priv->initialized)
		return 0;

	xps_ll_temac_init(dev, bis);

	printf("%s: Xilinx XPS LocalLink Tri-Mode Ether MAC #%d at 0x%08X.\n",
		dev->name, 0, dev->iobase);

#if DEBUG
	for (i = 0; i < 32; i++)
		read_phy_reg(dev, i);
#endif

	if (!xps_ll_temac_phy_ctrl(dev)) {
		ll_temac_halt(dev);
		return -1;
	}

	priv->initialized = 1;
	return 0;
}

/* mode bits: 0bit - fifo(0)/sdma(1), 1bit - no dcr(0)/dcr(1)
 * ctrl - control address for file/sdma */
int xilinx_ll_temac_initialize(bd_t *bis, unsigned long base_addr,
							int mode, int ctrl)
{
	struct eth_device *dev;
	struct ll_priv *priv;

	dev = calloc(1, sizeof(*dev));
	if (dev == NULL)
		return -1;

	dev->priv = calloc(1, sizeof(struct ll_priv));
	if (dev->priv == NULL) {
		free(dev);
		return -1;
	}

	priv = dev->priv;

	sprintf(dev->name, "Xlltem.%lx", base_addr);

	dev->iobase = base_addr;
	priv->ctrl = ctrl;
	priv->mode = mode;

#ifdef CONFIG_PHY_ADDR
	priv->phyaddr = CONFIG_PHY_ADDR;
#else
	priv->phyaddr = -1;
#endif

	dev->init = ll_temac_init;
	dev->halt = ll_temac_halt;
	dev->write_hwaddr = ll_temac_addr_setup;

	if (priv->mode & 0x1) {
		dev->send = ll_temac_send_sdma;
		dev->recv = ll_temac_recv_sdma;
	} else {
		dev->send = ll_temac_send_fifo;
		dev->recv = ll_temac_recv_fifo;
	}

	eth_register(dev);

	return 1;
}
