/******************************************************************************
*
* (c) Copyright 2010 Xilinx, Inc. All rights reserved.
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
* @file xaxidma.h
*
* This is the driver API for the AXI DMA engine.
*
* For a full description of DMA features, please see the hardware spec. This
* driver supports the following features:
*
*   - Scatter-Gather DMA (SGDMA)
*   - Interrupts
*   - Programmable interrupt coalescing for SGDMA
*   - APIs to manage Buffer Descriptors (BD) movement to and from the SGDMA
*     engine
*
* <b>Transactions</b>
*
* The object used to describe a transaction is referred to as a Buffer
* Descriptor (BD). Buffer descriptors are allocated in the user application.
* The user application needs to set buffer address, transfer length, and
* control information for this transfer. The control information includes
* SOF and EOF. Definition of those masks are in xaxidma_hw.h
*
* <b>Scatter-Gather DMA</b>
*
* SGDMA allows the application to define a list of transactions in memory which
* the hardware will process without further application intervention. During
* this time, the application is free to continue adding more work to keep the
* Hardware busy.
*
* User can check for the completion of transactions through polling the
* hardware, or interrupts.
*
* SGDMA processes whole packets. A packet is defined as a series of
* data bytes that represent a message. SGDMA allows a packet of data to be
* broken up into one or more transactions. For example, take an Ethernet IP
* packet which consists of a 14 byte header followed by a 1 or more bytes of
* payload. With SGDMA, the application may point a BD to the header and another
* BD to the payload, then transfer them as a single message. This strategy can
* make a TCP/IP stack more efficient by allowing it to keep packet header and
* data in different memory regions instead of assembling packets into
* contiguous blocks of memory.
*
* <b>BD Ring Management</b>
*
* BD rings are shared by the software and the hardware.
*
* The hardware expects BDs to be setup as a linked list. The DMA hardware walks
* through the list by following the next pointer field of a completed BD.  
* The hardware stops processing when the just completed BD is the same as the
* BD specified in the Tail Ptr register in the hardware.
*
* The last BD in the ring is linked to the first BD in the ring.
*
* All BD management are done inside the driver. The user application should not
* directly modify the BD fields. Modifications to the BD fields should always
* go through the specific API functions.
*
* Within the ring, the driver maintains four groups of BDs. Each group consists
* of 0 or more adjacent BDs:
*
*   - Free: The BDs that can be allocated by the application with
*     XAxiDma_BdRingAlloc().  
*
*   - Pre-process: The BDs that have been allocated with
*     XAxiDma_BdRingAlloc(). These BDs are under application control. The
*     application modifies these BDs through driver API to prepare them 
*     for DMA transactions.
*
*   - Hardware: The BDs that have been enqueued to hardware with
*     XAxiDma_BdRingToHw(). These BDs are under hardware control and may be in a
*     state of awaiting hardware processing, in process, or processed by
*     hardware. It is considered an error for the application to change BDs
*     while they are in this group. Doing so can cause data corruption and lead
*     to system instability.
*
*   - Post-process: The BDs that have been processed by hardware and have
*     been extracted from the Hardware group with XAxiDma_BdRingFromHw(). 
*     These BDs are under application control. The application can check the
*     transfer status of these BDs. The application use XAxiDma_BdRingFree()
*     to put them into the Free group.
*
* BDs are expected to transition in the following way for continuous 
* DMA transfers:
* <pre>
*
*         XAxiDma_BdRingAlloc()                   XAxiDma_BdRingToHw()
*   Free ------------------------> Pre-process ----------------------> Hardware
*                                                                      |
*    /|\                                                               |
*     |   XAxiDma_BdRingFree()                  XAxiDma_BdRingFromHw() |
*     +--------------------------- Post-process <----------------------+
*
* </pre>
*
* When a DMA transfer is to be cancelled before enqueuing to hardware,
* application can return the requested BDs to the Free group using
* XAxiDma_BdRingUnAlloc(), as shown below:
* <pre>
*
*         XAxiDma_BdRingUnAlloc()
*   Free <----------------------- Pre-process
*
* </pre>
*
* The API provides functions for BD list traversal:
* - XAxiDma_BdRingNext()
* - XAxiDma_BdRingPrev()
*
* These functions should be used with care as they do not understand where
* one group ends and another begins.
*
* <b>SGDMA Descriptor Ring Creation</b>
*
* BD ring is created using XAxiDma_BdRingCreate(). The memory for the BD ring
* is allocated by the application, and it has to be contiguous. Physical
* address is required to setup the BD ring. 
*
* The applicaiton can use XAxiDma_BdRingMemCalc() to find out the amount of 
* memory needed for a certain number of BDs. XAxiDma_BdRingCntCalc() can be
* used to find out how many BDs can be allocated for certain amount of memory.
* 
* A helper function, XAxiDma_BdRingClone(), can speed up the BD ring setup if
* the BDs have same types of controls, for example, SOF and EOF. After
* using the XAxiDma_BdRingClone(), the application only needs to setup the
* buffer address and transfer length. Note that certain BDs in one packet,
* for example, the first BD and the last BD, may need to setup special
* control information.
*
* <b>Descriptor Ring State Machine</b>
*
* There are two states of the BD ring:
*
*   - HALTED (H), where hardware is not running
*
*   - NOT HALTED (NH), where hardware is running
*
* The following diagram shows the state transition for the DMA engine:
*
* <pre>
*   _____ XAxiDma_StartBdRingHw(), or XAxiDma_BdRingStart(),   ______
*   |   |               or XAxiDma_Resume()                    |    |
*   | H |----------------------------------------------------->| NH |
*   |   |<-----------------------------------------------------|    |           
*   -----   XAxiDma_Pause() or XAxiDma_Reset()                 ------
* </pre>
*
* <b>Interrupt Coalescing</b>
*
* SGDMA provides control over the frequency of interrupts through interrupt
* coalescing. The DMA engine provides two ways to tune the interrupt 
* coalescing:
*
* - The packet threshold counter. Interrupt will fire once the
*   programmable number of packets have been processed by the engine.
*
* - The packet delay timer counter. Interrupt will fire once the
*   programmable amount of time has passed after processing the last packet, 
*   and no new packets to process. Note that the interrupt will only fire if
*   at least one packet has been processed.
*
* <b> Interrupt </b>
*
* Interrupts are handled by the user application. Each DMA channel has its own
* interrupt ID. The driver provides APIs to enable/disable interrupt,
* and tune the interrupt frequency regarding to packet processing frequency.
*
* <b> Software Initialization </b>
*
* To use the DMA engine for transfers, the following setup are required:
*
* - DMA Initialization using XAxiDma_CfgInitialize() function. This step
*   initializes a driver instance for the given DMA engine and resets the
*   engine.
*
* - BD Ring creation. A BD ring is needed per DMA channel and can be built by
*   calling XAxiDma_BdRingCreate().
*
* - Enable interrupts if chose to use interrupt mode. The application is
*   responsible for setting up the interrupt system, which includes providing
*   and connecting interrupt handlers and call back functions, before
*   enabling the interrupts.
*
* - Start a DMA transfer: Call XAxiDma_BdRingStart() to start a transfer for
*   the first time or after a reset, and XAxiDma_BdRingToHw() if the channel
*   is already started. Calling XAxiDma_BdRingToHw() when a DMA channel is not
*   running will not put the BDs to the hardware, and the BDs will be processed
*   later when the DMA channel is started through XAxiDma_BdRingStart().
*
* <b> How to start DMA transactions </b>
*
* The user application uses XAxiDma_BdRingToHw() to submit BDs to the hardware
* to start DMA transfers.
*
* For both channels, if the DMA engine is currently stopped (using
* XAxiDma_Pause()), the newly added BDs will be accepted but not processed
* until the DMA engine is started, using XAxiDma_BdRingStart(), or resumed,
* using XAxiDma_Resume(). 
*
* <b> Software Post-Processing on completed DMA transactions </b>
*
* If the interrupt system has been set up and the interrupts are enabled, 
* a DMA channels notifies the software about the completion of a transfer
* through interrupts. Otherwise, the user application can poll for
* completions of the BDs, using XAxiDma_BdRingFromHw() or
* XAxiDma_BdHwCompleted(). 
*
* - Once BDs are finished by a channel, the application first needs to fetch
*   them from the channel using XAxiDma_BdRingFromHw().
*
* - On the TX side, the application now could free the data buffers attached to
*   those BDs as the data in the buffers has been transmitted.
*
* - On the RX side, the application now could use the received data in the 
*	buffers attached to those BDs.
*
* - For both channels, completed BDs need to be put back to the Free group
*   using XAxiDma_BdRingFree(), so they can be used for future transactions.
*
* - On the RX side, it is the application's responsibility to have BDs ready
*   to receive data at any time. Otherwise, the RX channel refuses to
*   accept any data if it has no RX BDs.
*
* <b> Examples </b>
*
* We provide three examples to show how to use the driver API:
* - One for interrupt mode (example_intr.c), multiple BD/packets transfer
* - One for polling mode (example_poll.c), single BD transfer.
* - One for polling mode (poll_multi_pkts.c), multiple BD/packets transfer
*
* <b> Address Translation </b>
*
* All buffer addresses and BD addresses for the hardware are physical 
* addresses. The user application is responsible to provide physical buffer
* address for the BD upon BD ring creation. The user application accesses BD
* through its virtual addess. The driver maintains the address translation
* between the physical and virtual address for BDs.
*
* <b> Cache Coherency </b>
*
* This driver expects all application buffers attached to BDs to be in cache
* coherent memory. If cache is used in the system, buffers for transmit MUST
* be flushed from the cache before passing the associated BD to this driver.
* Buffers for receive MUST be invalidated before accessing the data. 
*
* <b> Alignment </b>
*
* For BDs:
*
* Minimum alignment is defined by the constant XAXIDMA_BD_MINIMUM_ALIGNMENT.
* This is the smallest alignment allowed by both hardware and software for them
* to properly work.
*
* If the descriptor ring is to be placed in cached memory, alignment also MUST
* be at least the processor's cache-line size. Otherwise, system instability 
* occurs. For alignment larger than the cache line size, multiple cache line
* size alignment is required.
*
* Aside from the initial creation of the descriptor ring (see
* XAxiDma_BdRingCreate()), there are no other run-time checks for proper
* alignment of BDs.
*
* For application data buffers:
*
* Application data buffers may reside on any alignment if DRE is built into the
* hardware. Otherwise, application data buffer must be word-aligned. The word
* is defined by XPAR_AXIDMA_0_M_AXIS_MM2S_TDATA_WIDTH for transmit and
* XPAR_AXIDMA_0_S_AXIS_S2MM_TDATA_WIDTH for receive.
*
* For scatter gather transfers that have more than one BDs in the chain of BDs,
* Each BD transfer length must be multiple of word too. Otherwise, internal
* error happens in the hardware.
* 
* <b> Error Handling </b>
*
* The DMA engine will halt on all error conditions. It requires the software
* to do a reset before it can start process new transfer requests.
*
* <b> Restart After Stopping </b>
*
* After the DMA engine has been stopped (through reset or reset after an error)
* the software keeps track of the current BD pointer when reset happens, and
* processing of BDs can be resumed through XAxiDma_BdRingStart().
*
* <b> Limitations </b>
*
* This driver does not have any mechanisms for mutual exclusion. It is up to
* the application to provide this protection.
*
* <b> Hardware Defaults & Exclusive Use </b>
*
* After the initialization or reset, the DMA engine is in the following
* default mode:
* - All interrupts are disabled.
* 
* - Interrupt coalescing counter is 1.
*
* - The DMA engine is not running (halted). Each DMA channel is started 
*   separately, using XAxiDma_StartBdRingHw() if no BDs are setup for transfer
*   yet, or XAxiDma_BdRingStart() otherwise.	
*
* The driver has exclusive use of the registers and BDs. All accesses to the
* registers and BDs should go through the driver interface.
*
* <b> Debug Print </b>
*
* To see the debug print for the driver, please put "-DDEBUG" as the extra
* compiler flags in software platform settings. Also comment out the line in
* xdebug.h: "#undef DEBUG".
*
* <b>Changes From v1.00a</b>
*
* . We have changes return type for XAxiDma_BdSetBufAddr() from void to int
* . We added XAxiDma_LookupConfig() so that user does not need to look for the
* hardware settings anymore.
*
* <pre>
* MODIFICATION HISTORY:
*
* Ver   Who  Date     Changes
* ----- ---- -------- -------------------------------------------------------
* 1.00a jz   05/18/10 First release
* 2.00a jz   08/10/10 Second release, added in xaxidma_g.c, xaxidma_sinit.c,
*                     updated tcl file, added xaxidma_porting_guide.h
* 3.00a jz   11/22/10 Support IP core parameters change
* </pre>
*
******************************************************************************/

#ifndef XAXIDMA_H_   /* prevent circular inclusions */
#define XAXIDMA_H_

#ifdef __cplusplus
extern "C" {
#endif

/***************************** Include Files *********************************/
#include "xaxidma_bdring.h"

/************************** Constant Definitions *****************************/

/**************************** Type Definitions *******************************/

/**
 * The XAxiDma driver instance data. An instance must be allocated for each DMA
 * engine in use.
 */
typedef struct XAxiDma {
	u32 RegBase;             /* Virtual base address of DMA engine */
	
	int HasMm2S;             /* Has transmit channel */
	int HasS2Mm;             /* Has receive channel */
	int Initialized;         /* Driver has been initialized */

	XAxiDma_BdRing TxBdRing; /* BD container management for TX channel */
	XAxiDma_BdRing RxBdRing; /* BD container management for RX channel */
} XAxiDma;

/**
 * The configuration structure for AXI DMA engine
 *
 * This structure passes the hardware building information to the driver
 */
typedef struct {
	int DeviceId;
	u32 BaseAddr;

	int HasStsCntrlStrm;
	int HasMm2S;
	int HasMm2SDRE;
	int Mm2SDataWidth;
	int HasS2Mm;
	int HasS2MmDRE;
	int S2MmDataWidth;
} XAxiDma_Config;


/***************** Macros (Inline Functions) Definitions *********************/
/*****************************************************************************/
/**
* Get Transmit (Tx) Ring ptr
*
* Warning: This has a different API than the LLDMA driver. It now returns
* the pointer to the BD ring.
* 
* @param  InstancePtr is a pointer to the DMA engine instance to be worked on.
*
* @return 
*	Pointer to the Tx Ring
* @note
* C-style signature:
* XAxiDma_BdRing * XAxiDma_GetTxRing(XAxiDma * InstancePtr)
*
*****************************************************************************/
#define XAxiDma_GetTxRing(InstancePtr) (&((InstancePtr)->TxBdRing))

/*****************************************************************************/
/**
* Get Receive (Rx) Ring ptr
* 
* Warning: This has a different API than the LLDMA driver. It now returns
* the pointer to the BD ring.
* 
* @param  InstancePtr is a pointer to the DMA engine instance to be worked on.
*
* @return 
*	Pointer to the Rx Ring
*
* @note
* C-style signature:
* XAxiDma_BdRing * XAxiDma_GetRxRing(XAxiDma * InstancePtr)
*
*****************************************************************************/
#define XAxiDma_GetRxRing(InstancePtr) (&((InstancePtr)->RxBdRing))

/************************** Function Prototypes ******************************/

/*
 * Initialization and control functions in xaxidma.c
 */
XAxiDma_Config *XAxiDma_LookupConfig(u32 DeviceId);
int XAxiDma_CfgInitialize(XAxiDma * InstancePtr, XAxiDma_Config *Config);
void XAxiDma_Reset(XAxiDma * InstancePtr);
int XAxiDma_ResetIsDone(XAxiDma * InstancePtr);
int XAxiDma_Pause(XAxiDma * InstancePtr);
int XAxiDma_Resume(XAxiDma * InstancePtr);

#ifdef __cplusplus
}
#endif

#endif /* end of protection macro */
