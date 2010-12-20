/* $Id: xio.c,v 1.1.2.1 2010/06/16 08:09:08 sadanan Exp $ */
/******************************************************************************
*
* (c) Copyright 2007-2009 Xilinx, Inc. All rights reserved.
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
* @file xio.c
*
* Contains I/O functions for memory-mapped or non-memory-mapped I/O
* architectures.  These functions encapsulate generic CPU I/O requirements.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date	 Changes
* ----- ---- -------- -------------------------------------------------------
* 1.00a rpm  11/07/03 Added InSwap/OutSwap routines for endian conversion
* 1.01a ecm  02/24/06 CR225908 corrected the extra curly braces in macros
*                     and bumped version to 1.01.a.
* 2.11a mta  03/21/07 Updated to new coding style.
*
* </pre>
*
* @note
*
* This file may contain architecture-dependent code.
*
******************************************************************************/

/***************************** Include Files *********************************/

#include "xio.h"
#include "xbasic_types.h"

/************************** Constant Definitions *****************************/


/**************************** Type Definitions *******************************/


/***************** Macros (Inline Functions) Definitions *********************/


/************************** Function Prototypes ******************************/


/*****************************************************************************/
/**
*
* Performs a 16-bit endian converion.
*
* @param	Source contains the value to be converted.
* @param	DestPtr contains a pointer to the location to put the
*		converted value.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
void XIo_EndianSwap16(u16 Source, u16 *DestPtr)
{
	*DestPtr = (u16) (((Source & 0xFF00) >> 8) | ((Source & 0x00FF) << 8));
}

/*****************************************************************************/
/**
*
* Performs a 32-bit endian converion.
*
* @param	Source contains the value to be converted.
* @param	DestPtr contains a pointer to the location to put the
*		converted value.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
void XIo_EndianSwap32(u32 Source, u32 *DestPtr)
{
	/* get each of the half words from the 32 bit word */

	u16 LoWord = (u16) (Source & 0x0000FFFF);
	u16 HiWord = (u16) ((Source & 0xFFFF0000) >> 16);

	/* byte swap each of the 16 bit half words */

	LoWord = (((LoWord & 0xFF00) >> 8) | ((LoWord & 0x00FF) << 8));
	HiWord = (((HiWord & 0xFF00) >> 8) | ((HiWord & 0x00FF) << 8));

	/* swap the half words before returning the value */

	*DestPtr = (u32) ((LoWord << 16) | HiWord);
}

/*****************************************************************************/
/**
*
* Performs an input operation for a 16-bit memory location by reading from the
* specified address and returning the byte-swapped value read from that
* address.
*
* @param	InAddress contains the address to perform the input
*		operation at.
*
* @return	The byte-swapped value read from the specified input address.
*
* @note		None.
*
******************************************************************************/
u16 XIo_InSwap16(XIo_Address InAddress)
{
	u16 InData;

	/* get the data then swap it */
	InData = XIo_In16(InAddress);

	return (u16) (((InData & 0xFF00) >> 8) | ((InData & 0x00FF) << 8));
}

/*****************************************************************************/
/**
*
* Performs an input operation for a 32-bit memory location by reading from the
* specified address and returning the byte-swapped value read from that
* address.
*
* @param	InAddress contains the address to perform the input
*		operation at.
*
* @return	The byte-swapped value read from the specified input address.
*
* @note		None.
*
******************************************************************************/
u32 XIo_InSwap32(XIo_Address InAddress)
{
	u32 InData;
	u32 SwapData;

	/* get the data then swap it */
	InData = XIo_In32(InAddress);
	XIo_EndianSwap32(InData, &SwapData);

	return SwapData;
}

/*****************************************************************************/
/**
*
* Performs an output operation for a 16-bit memory location by writing the
* specified value to the the specified address. The value is byte-swapped
* before being written.
*
* @param	OutAddress contains the address to perform the output
*		operation at.
* @param	Value contains the value to be output at the specified address.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
void XIo_OutSwap16(XIo_Address OutAddress, u16 Value)
{
	u16 OutData;

	/* swap the data then output it */
	OutData = (u16) (((Value & 0xFF00) >> 8) | ((Value & 0x00FF) << 8));

	XIo_Out16(OutAddress, OutData);
}

/*****************************************************************************/
/**
*
* Performs an output operation for a 32-bit memory location by writing the
* specified value to the the specified address. The value is byte-swapped
* before being written.
*
* @param	OutAddress contains the address at which the
*		output operation has to be done.
* @param	Value contains the value to be output at the specified address.
*
* @return	None.
*
* @note		None.
*
******************************************************************************/
void XIo_OutSwap32(XIo_Address OutAddress, u32 Value)
{
	u32 OutData;

	/* swap the data then output it */
	XIo_EndianSwap32(Value, &OutData);
	XIo_Out32(OutAddress, OutData);
}
