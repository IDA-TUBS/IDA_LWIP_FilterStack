/******************************************************************************
*
* Copyright (C) 2011 - 2016 Xilinx, Inc.  All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
* XILINX  BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
* OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*
* Except as contained in this notice, the name of the Xilinx shall not be used
* in advertising or otherwise to promote the sale, use or other dealings in
* this Software without prior written authorization from Xilinx.
*
******************************************************************************/

#ifndef XEMACPS_IEEE1588_H
#define XEMACPS_IEEE1588_H

#ifdef __cplusplus
extern "C" {
#endif


/***************************** Include Files *********************************/

#include "xemacpsif.h"

/************************** Constant Definitions *****************************/

#define NS_PER_SEC 1000000000ULL      /* Nanoseconds per second */
#define FP_MULT    1000ULL

// PTP Capture Register
#define XEMACPS_PTP_TSU_TX_SEC_OFFSET			0x000001E0				// PTP TSU Tx Seconds Capture Register Offset
#define XEMACPS_PTP_TSU_TX_NSEC_OFFSET			0x000001E4				// PTP TSU Tx NSeconds Capture Register Offset
#define XEMACPS_PTP_TSU_RX_SEC_OFFSET			0x000001E8				// PTP TSU Rx Seconds Capture Register Offset
#define XEMACPS_PTP_TSU_RX_NSEC_OFFSET 			0x000001EC				// PTP TSU Rx NSeconds Capture Register Offset

#define XEMACPS_NWCTRL_OSSM_MASK 				0x01000000				// One Step Sync
#define XEMACPS_PTP_INT_SYNC_RX_MASK 			0x00080000				// Mask PTP Rx interrupts
#define XEMACPS_PTP_INT_SYNC_TX_MASK 			0x00200000				// Mask PTP Tx interrupts

/*
 * Timer Counter Register
 */
#define XEMACPS_PTP_TSU_SEC_REG_OFFSET 			0x000001D0				// TSU Timer Seconds Register Offset
#define XEMACPS_PTP_TSU_NSEC_REG_OFFSET 		0x000001D4				// TSU Timer NSeconds Register Offset

/*
 * Increment Register
 */
#define XEMACPS_PTP_TSU_SUB_INC_OFFSET			0x000001BC				// TSU Sub-Nanoseconds Increment Reg. Offset
#define XEMACPS_PTP_TSU_INC_OFFSET				0x000001DC				// TSU Nanoseconds Increment Reg. Offset
#define XEMACPS_PTP_TSU_ADJUST_OFFSET			0x000001D8				// TSU Adjust Register Offset
#define XEMACPS_PTP_TSU_NS_INCR_SIZE			8						// Size of the NSeconds Increment
#define XEMACPS_PTP_TSU_SUB_NS_INCR_MSB_OFFSET	0 						// sub-ns MSB [23:8]
#define XEMACPS_PTP_TSU_SUB_NS_INCR_MSB_SIZE	16						// Size of the Sub-NSeconds MSB Increment
#define XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_OFFSET	24 						// sub-ns MSB [7:0]
#define XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_SIZE	8						// Size of the Sub-NSeconds LSB Increment
#define XEMACPS_PTP_TSU_SUB_NS_INCR_SIZE 		(XEMACPS_PTP_TSU_SUB_NS_INCR_MSB_SIZE + XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_SIZE) /* 16 + 8 */

/*
 * Mask for shifting to subns register
 */
#define XEMACPS_PTP_TSU_SUB_NS_LSB_MASK                        0xFF		// Masks the LSB of the subns Register
#define XEMACPS_PTP_TSU_SUB_NS_MSB_MASK                        0xFFFF00	// Masks the MSB of the subns register

#define ETH_PTP_PositiveTime      ((uint32_t)0x00000000)  /*!< Positive time value */
#define ETH_PTP_NegativeTime      ((uint32_t)0x80000000)  /*!< Negative time value */


/**************************** Type Definitions *******************************/

struct ptptime_t {
  s32_t tv_sec;
  s32_t tv_nsec;
};

typedef struct {
	u32 nanoseconds;
	u32 subnanoseconds;
}XEmacPs_Tsu_incr;


/***************** Macros (Inline Functions) Definitions *********************/


/************************** Function Prototypes ******************************/

void XEmacPs_InitTsu(void);
void XEmacPs_EnableChecksumOffload(xemacpsif_s *xemacpsif);
void XEmacPs_initPtp(xemacpsif_s *xemacpsif);
void XEmacPs_GetRxTimestamp(void);
void XEmacPs_GetTxTimestamp(void);
u32 XEmacPs_TsuCalcClk(u32 Freq);
void XEmacPs_WriteTsuIncr(u32 ns, u32 subns);
XEmacPs_Tsu_incr XEmacPs_ReadTsuIncr(void);


/*** Prototypes for PTPd Interface */
void ETH_SetPTPTimeStampUpdate(uint32_t Sign, uint32_t SecondValue, uint32_t NanoSecondValue);
void ETH_PTPTime_UpdateOffset(struct ptptime_t * timeoffset);
void ETH_SetPTPTimeStampAddend(uint32_t Value);
void ETH_EnablePTPTimeStampAddend(void);
void ETH_PTPTime_AdjFreq(int32_t Adj);
void ETH_PTPTime_SetTime(struct ptptime_t * timestamp);
void ETH_PTPTime_GetTime(struct ptptime_t* timestamp);


#ifdef __cplusplus
}
#endif

#endif /* end of protection macro */
