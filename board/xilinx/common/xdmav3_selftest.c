/* $Id: */
/******************************************************************************
*
*       (c) Copyright 2006 Xilinx Inc.
*       All rights reserved.
* This program is free software; you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published by the
* Free Software Foundation; either version 2 of the License, or (at your
* option) any later version.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*
******************************************************************************/
/*****************************************************************************/
/**
*
* @file xdmav3_selftest.c
*
* This file implements DMA selftest related functions. For more
* information on this driver, see xdmav3.h.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -------------------------------------------------------
* 3.00a rmm  03/11/06 First release
* </pre>
******************************************************************************/

/***************************** Include Files *********************************/

#include "xdmav3.h"

/************************** Constant Definitions *****************************/


/**************************** Type Definitions *******************************/


/***************** Macros (Inline Functions) Definitions *********************/


/************************** Function Prototypes ******************************/


/************************** Variable Definitions *****************************/


/*****************************************************************************/
/**
* Selftest is not implemented.
*
* @param InstancePtr is a pointer to the instance to be worked on.
*
* @return
* - XST_SUCCESS if the self test passes
*
******************************************************************************/
int XDmaV3_SelfTest(XDmaV3 * InstancePtr)
{
	return (XST_SUCCESS);
}
