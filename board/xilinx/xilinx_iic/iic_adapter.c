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
*     (c) Copyright 2002-2007 Xilinx Inc.
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
#include <environment.h>
#include <i2c.h>
#include "xiic_l.h"

#if defined(CONFIG_CMD_I2C) || defined(CFG_ENV_IS_IN_EEPROM)

/************************************************************************
 * Send data to a device on i2c bus
 */
static int
send(uchar chip, u32 adr, int alen, u8 * data, u32 len)
{
	u8 sendBuf[4+len];
	u32 ret = 0;

	/* Put address and data bits together */
	sendBuf[0] = (u8) (adr >> 24);
	sendBuf[1] = (u8) (adr >> 16);
	sendBuf[2] = (u8) (adr >> 8);
	sendBuf[3] = (u8) adr;

	memcpy(&sendBuf[4], data, len);

	/* Send to device through iic bus */
	ret = XIic_Send(XPAR_IIC_0_BASEADDR, chip, &sendBuf[4-alen], 
			len + alen);

	if (ret != (len+alen))
		return 1;

	return 0;
}

/************************************************************************
 * Read data from a device on i2c bus
 */
static int
receive(uchar chip, u32 adr, int alen, u8 * data, u32 len)
{
	u8 address[4];
	u32 ret = 0;

	address[0] = (u8) (adr >> 24);
	address[1] = (u8) (adr >> 16);
	address[2] = (u8) (adr >> 8);
	address[3] = (u8) adr;

	/* Provide device address */
	ret = XIic_Send(XPAR_IIC_0_BASEADDR, chip, &address[4-alen], alen);
	if (ret != alen)
		return 1;
	
	/* Receive data from i2c device */
	ret = XIic_Recv(XPAR_IIC_0_BASEADDR, chip, data, len);
	if (ret != len)
		return 1;

	return 0;
}

/************************************************************************
 * U-boot call for i2c read associated activities.
 */
int
i2c_read(uchar chip, uint addr, int alen, uchar * buffer, int len)
{
	/* Address length should be smaller than 4 and greater than 0 */
	if ((alen > 4) || (alen < 0)) {
		printf("I2C read: addr len %d not supported\n", alen);
		return 1;
	}

#ifdef CFG_I2C_EEPROM_ADDR_OVERFLOW
	/*
	 * EEPROM chips that implement "address overflow" are ones
	 * like Catalyst 24WC04/08/16 which has 9/10/11 bits of
	 * address and the extra bits end up in the "chip address"
	 * bit slots. This makes a 24WC08 (1Kbyte) chip look like
	 * four 256 byte chips.
	 *
	 * Note that we consider the length of the address field to
	 * still be one byte because the extra address bits are
	 * hidden in the chip address.
	 */
	if( alen > 0 )
		chip |= ((addr >> (alen * 8)) & CFG_I2C_EEPROM_ADDR_OVERFLOW);
#endif
	return(receive(chip, addr, alen, buffer, len));
}

/************************************************************************
 * U-boot call for i2c write acativities.
 */
int
i2c_write(uchar chip, uint addr, int alen, uchar * buffer, int len)
{
	/* Address length should be smaller than 4 and greater than 0 */
	if ((alen > 4) || (alen < 0)) {
		printf("I2C write: addr len %d not supported\n", alen);
		return 1;
	}

#ifdef CFG_I2C_EEPROM_ADDR_OVERFLOW
	/*
	 * EEPROM chips that implement "address overflow" are ones
	 * like Catalyst 24WC04/08/16 which has 9/10/11 bits of
	 * address and the extra bits end up in the "chip address"
	 * bit slots. This makes a 24WC08 (1Kbyte) chip look like
	 * four 256 byte chips.
	 *
	 * Note that we consider the length of the address field to
	 * still be one byte because the extra address bits are
	 * hidden in the chip address.
	 */
	if( alen > 0 )
		chip |= ((addr >> (alen * 8)) & CFG_I2C_EEPROM_ADDR_OVERFLOW);
#endif

	return(send(chip, addr, alen, buffer, len));
}

/************************************************************************
 * Probing devices available on i2c bus
 */
int
i2c_probe(uchar chip)
{
	u8 data;

	return (receive(chip, 0, 1, &data, 1));
}

#endif /* CFG_ENV_IS_IN_EEPROM || defined(CFG_COMMAND_I2C) */
