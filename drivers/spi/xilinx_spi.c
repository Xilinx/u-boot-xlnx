/*
 * Copyright (C) 2007 Atmel Corporation
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
#include <common.h>
#include <spi.h>
#include <malloc.h>
#include <asm/io.h>

//#define DEBUG_ENV
#ifdef DEBUG_ENV
#define DEBUGF(fmt,args...)	printf(fmt,##args)
#else
#define DEBUGF(fmt,atgs...)
#endif

#define XSPI_CR_OFFSET		0x62	/* 16-bit Control Register */

#define XSPI_CR_ENABLE		0x02
#define XSPI_CR_MASTER_MODE	0x04
#define XSPI_CR_CPOL		0x08
#define XSPI_CR_CPHA		0x10
#define XSPI_CR_MODE_MASK	(XSPI_CR_CPHA | XSPI_CR_CPOL)
#define XSPI_CR_TXFIFO_RESET	0x20
#define XSPI_CR_RXFIFO_RESET	0x40
#define XSPI_CR_MANUAL_SSELECT	0x80
#define XSPI_CR_TRANS_INHIBIT	0x100

#define XSPI_SR_OFFSET		0x67	/* 8-bit Status Register */

#define XSPI_SR_RX_EMPTY_MASK	0x01	/* Receive FIFO is empty */
#define XSPI_SR_RX_FULL_MASK	0x02	/* Receive FIFO is full */
#define XSPI_SR_TX_EMPTY_MASK	0x04	/* Transmit FIFO is empty */
#define XSPI_SR_TX_FULL_MASK	0x08	/* Transmit FIFO is full */
#define XSPI_SR_MODE_FAULT_MASK	0x10	/* Mode fault error */

#define XSPI_TXD_OFFSET		0x6b	/* 8-bit Data Transmit Register */
#define XSPI_RXD_OFFSET		0x6f	/* 8-bit Data Receive Register */

#define XSPI_SSR_OFFSET		0x70	/* 32-bit Slave Select Register */

/* Register definitions as per "OPB IPIF (v3.01c) Product Specification", DS414
 * IPIF registers are 32 bit
 */
#define XIPIF_V123B_DGIER_OFFSET	0x1c	/* IPIF global int enable reg */
#define XIPIF_V123B_GINTR_ENABLE	0x80000000

#define XIPIF_V123B_IISR_OFFSET		0x20	/* IPIF interrupt status reg */
#define XIPIF_V123B_IIER_OFFSET		0x28	/* IPIF interrupt enable reg */

#define XSPI_INTR_MODE_FAULT		0x01	/* Mode fault error */
#define XSPI_INTR_SLAVE_MODE_FAULT	0x02	/* Selected as slave while
						 * disabled */
#define XSPI_INTR_TX_EMPTY		0x04	/* TxFIFO is empty */
#define XSPI_INTR_TX_UNDERRUN		0x08	/* TxFIFO was underrun */
#define XSPI_INTR_RX_FULL		0x10	/* RxFIFO is full */
#define XSPI_INTR_RX_OVERRUN		0x20	/* RxFIFO was overrun */

#define XIPIF_V123B_RESETR_OFFSET	0x40	/* IPIF reset register */
#define XIPIF_V123B_RESET_MASK		0x0a	/* the value to write */


u32 BaseAddr = CONFIG_XILINX_SPI_BASEADDR;

void spi_init()
{

}

struct spi_slave *spi_setup_slave(unsigned int bus, unsigned int cs,
			unsigned int max_hz, unsigned int mode)
{
	struct spi_slave *xspi;
	u16 sr;
	xspi = malloc(sizeof(struct spi_slave));
	DEBUGF("%s[%d] spi BaseAddr: 0x%x \n",__FUNCTION__,__LINE__,BaseAddr);
	if (!xspi){
		DEBUGF("%s[%d] spi not allocated \n",__FUNCTION__,__LINE__);
		return NULL;
	}
	xspi->bus = bus;
	xspi->cs = cs;
	/* Reset the SPI device */
	out_8(BaseAddr + XIPIF_V123B_RESETR_OFFSET,
		 XIPIF_V123B_RESET_MASK);
	DEBUGF("%s[%d] SPI Reset \n",__FUNCTION__,__LINE__);
	
	sr = in_8(BaseAddr + XSPI_SR_OFFSET);
	DEBUGF("%s[%d] transfer status reg %x \n",__FUNCTION__,__LINE__,sr);
	
	/* Disable all the interrupts just in case */
	out_8(BaseAddr + XIPIF_V123B_IIER_OFFSET, 0);
	sr = in_8(BaseAddr + XSPI_SR_OFFSET);
	DEBUGF("%s[%d] transfer status reg %x \n",__FUNCTION__,__LINE__,sr);
	
	/* Disable the transmitter, enable Manual Slave Select Assertion,
	 * put SPI controller into master mode, and enable it */
	out_be16(BaseAddr + XSPI_CR_OFFSET,
		 XSPI_CR_TRANS_INHIBIT | XSPI_CR_MANUAL_SSELECT
		 | XSPI_CR_MASTER_MODE | XSPI_CR_ENABLE);
	sr = in_be16(BaseAddr + XSPI_CR_OFFSET);
	DEBUGF("%s[%d] transfer control reg %x \n",__FUNCTION__,__LINE__,sr);

	return xspi;
}

void spi_free_slave(struct spi_slave *slave)
{
	free(slave);
	DEBUGF("SPI slave freed\n");
}

int spi_claim_bus(struct spi_slave *slave)
{
		// Toggle CS
		u8 sr;
	
		out_8(BaseAddr + XSPI_SSR_OFFSET, ~(0x0001 << slave->cs));
		//sr = in_8(BaseAddr + XSPI_SR_OFFSET);
		//DEBUGF("%s[%d] transfer status reg %x \n",__FUNCTION__,__LINE__,sr);
	
		return 0;
}

void spi_release_bus(struct spi_slave *slave)
{
	// Release Bus
	out_8(BaseAddr + XSPI_SSR_OFFSET, 0xffff);
	// Disable Device
	out_be16(BaseAddr + XSPI_CR_OFFSET,
		 XSPI_CR_TRANS_INHIBIT | XSPI_CR_MANUAL_SSELECT
		 | XSPI_CR_MASTER_MODE);
	DEBUGF("SPI bus released\n");
}

int spi_xfer(struct spi_slave *slave, unsigned int bitlen,
		const void *dout, void *din, unsigned long flags)
{
	unsigned int	len_tx;
	unsigned int	len_rx;
	unsigned int	len;
	int inhibited = 1;
	u8		sr,value;
	u16		cr;
	const u8	* txp = dout;
	u8		* rxp = din;

	if (bitlen == 0)
		/* Finish any previously submitted transfers */
		goto out;

	/*
	 * TODO: The controller can do non-multiple-of-8 bit
	 * transfers, but this driver currently doesn't support it.
	 *
	 * It's also not clear how such transfers are supposed to be
	 * represented as a stream of bytes...this is a limitation of
	 * the current SPI interface.
	 */
	if (bitlen % 8) {
		/* Errors always terminate an ongoing transfer */
		flags |= SPI_XFER_END;
		goto out;
	}

	len = bitlen / 8;

	/*
	 * The controller can do automatic CS control, but it is
	 * somewhat quirky, and it doesn't really buy us much anyway
	 * in the context of U-Boot.
	 */
	DEBUGF("%s[%d] going to claim cs\n",__FUNCTION__,__LINE__);
	if (flags & SPI_XFER_BEGIN)
		out_8(BaseAddr + XSPI_SSR_OFFSET, ~(0x0001 << slave->cs));
	sr = in_8(BaseAddr + XSPI_SR_OFFSET);
	DEBUGF("%s[%d] transfer status reg %x \n",__FUNCTION__,__LINE__,sr);
	
	for (len_tx = 0, len_rx = 0; len_tx < len; ) {
		while (!(sr & XSPI_SR_TX_FULL_MASK) & (len_tx<len))
		{
			// Write bytes
			if (len_tx < len) {
				if (txp) 
					out_8(BaseAddr + XSPI_TXD_OFFSET, *txp++);
				else 
					out_8(BaseAddr + XSPI_TXD_OFFSET, 0);
				len_tx++;
			}
			sr = in_8(BaseAddr + XSPI_SR_OFFSET);
			while (!(sr & XSPI_SR_RX_EMPTY_MASK)) {
				value = in_8(BaseAddr + XSPI_RXD_OFFSET);
				DEBUGF("%s[%d] received byte 0x%x\n ",__FUNCTION__,__LINE__,value);
				if (rxp)
					*rxp++ = value;
				len_rx++;
				sr = in_8(BaseAddr + XSPI_SR_OFFSET);
			}
		}
		//Activate

		out_be16(BaseAddr + XSPI_CR_OFFSET,
			 XSPI_CR_MANUAL_SSELECT
			 | XSPI_CR_MASTER_MODE | XSPI_CR_ENABLE);

		// Wait for transmitter to get empty
		do {
			sr = in_8(BaseAddr + XSPI_SR_OFFSET);
			DEBUGF("%s[%d] transfer status reg %x \n",__FUNCTION__,__LINE__,sr);
			while (!(sr & XSPI_SR_RX_EMPTY_MASK)) {
				value = in_8(BaseAddr + XSPI_RXD_OFFSET);
				DEBUGF("%s[%d] received byte 0x%x\n ",__FUNCTION__,__LINE__,value);
				if (rxp)
					*rxp++ = value;
				len_rx++;
				sr = in_8(BaseAddr + XSPI_SR_OFFSET);
			}
		} while (!(sr & XSPI_SR_TX_EMPTY_MASK));
	}

out:
	sr = in_8(BaseAddr + XSPI_SR_OFFSET);
	while (!(sr & XSPI_SR_RX_EMPTY_MASK)) {
		value = in_8(BaseAddr + XSPI_RXD_OFFSET);
		DEBUGF("%s[%d] received byte 0x%x\n ",__FUNCTION__,__LINE__,value);
		if (rxp)
			*rxp++ = value;
		len_rx++;
		sr = in_8(BaseAddr + XSPI_SR_OFFSET);
	}	

	out_be16(BaseAddr + XSPI_CR_OFFSET,
			 XSPI_CR_MANUAL_SSELECT	 | XSPI_CR_MASTER_MODE |
			 XSPI_CR_ENABLE |  XSPI_CR_TRANS_INHIBIT);

	DEBUGF("%s[%d] transfer inhibited \n",__FUNCTION__,__LINE__);
	if (flags & SPI_XFER_END) {
		/*
		 * Wait until the transfer is completely done before
		 * we deactivate CS and inhibit transmission.
		 */
		out_8(BaseAddr + XSPI_SSR_OFFSET, 0xffff);
	}
	return 0;
}
