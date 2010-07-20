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
* @file xlldma_userip.h
*
* This file is for the User-IP core (like Local-Link TEMAC) to define constants
* that are the User-IP core specific. DMA driver requires the constants to work
* correctly. Two constants must be defined in this file:
*
*   - XLLDMA_USR_APPWORD_OFFSET:
*
*     This constant defines a user word the User-IP always updates in the RX
*     Buffer Descriptors (BD) during any Receive transaction.
*
*     The DMA driver initializes this chosen user word of any RX BD to the
*     pre-defined value (see XLLDMA_USR_APPWORD_INITVALUE below) before
*     giving it to the RX channel. The DMA relies on its updation (by the
*     User-IP) to ensure the BD has been completed by the RX channel besides
*     checking the COMPLETE bit in XLLDMA_BD_STSCTRL_USR0_OFFSET field (see
*     xlldma_hw.h).
*
*     The only valid options for this constant are XLLDMA_BD_USR1_OFFSET,
*     XLLDMA_BD_USR2_OFFSET, XLLDMA_BD_USR3_OFFSET and XLLDMA_BD_USR4_OFFSET.
*
*     If the User-IP does not update any of the option fields above, the DMA
*     driver will not work properly.
*
*   - XLLDMA_USR_APPWORD_INITVALUE:
*
*     This constant defines the value the DMA driver uses to populate the
*     XLLDMA_USR_APPWORD_OFFSET field (see above) in any RX BD before giving
*     the BD to the RX channel for receive transaction.
*
*     It must be ensured that the User-IP will always populates a different
*     value from this constant into the XLLDMA_USR_APPWORD_OFFSET field at
*     the end of any receive transaction. Failing to do so will cause the
*     DMA driver to work improperly.
*
* If the User-IP uses different setting, the correct setting must be defined as
* a compiler options used in the Makefile. In either case the default
* definition of the constants in this file will be discarded.
*
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -------------------------------------------------------
* 1.00a xd   02/21/07 First release
* </pre>
*
******************************************************************************/

#ifndef XLLDMA_USERIP_H		/* prevent circular inclusions */
#define XLLDMA_USERIP_H		/* by using protection macros */

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/

#include "xlldma_hw.h"

/************************** Constant Definitions *****************************/

#ifndef XLLDMA_USERIP_APPWORD_OFFSET
#define XLLDMA_USERIP_APPWORD_OFFSET    XLLDMA_BD_USR4_OFFSET
#endif

#ifndef XLLDMA_USERIP_APPWORD_INITVALUE
#define XLLDMA_USERIP_APPWORD_INITVALUE 0xFFFFFFFF
#endif

/**************************** Type Definitions *******************************/

/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

#ifdef __cplusplus
}
#endif

#endif /* end of protection macro */
