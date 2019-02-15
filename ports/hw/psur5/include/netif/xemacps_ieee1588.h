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

#define DEBUG_XEMACPS_LEVEL1 /*
			      * Error messages which should be printed on
			      * console. It does not include error messages
			      * from interrupt service routines.
			      */
#undef DEBUG_LEVEL_TWO  /*
			      * Other debug messages, e.g. function names being
			      * executed, PTP protocl messages etc.
			      */

/* PTP Packet Message Type Definitions */
#define XEMACPS_PTP_TYPE_SYNC				0x0
#define XEMACPS_PTP_TYPE_FOLLOW_UP			0x8
#define XEMACPS_PTP_TYPE_PDELAYREQ			0x2
#define XEMACPS_PTP_TYPE_PDELAYRESP			0x3
#define XEMACPS_PTP_TYPE_PDELAYRESP_FOLLOW_UP		0xA
#define XEMACPS_PTP_TYPE_ANNOUNCE			0xB
#define XEMACPS_PTP_TYPE_SIGNALING			0xC


#define XEMACPS_PKT_TYPE_DISABLED			0xffff
#define XEMACPS_PKT_MAX_BUF_LEN				128



/* Standard PTP Frame Field Definitions (from IEEE1588 specification) */
#define XEMACPS_PTP_ETHERTYPE				0x88F7
#define XEMACPS_PTP_VERSION_PTP				2


/* Real Time Clock Definitions.*/
#define XEMACPS_ONE_SECOND				1000000000 /* In ns */

/*
 * Define how often to re-calculate the RTC Increment This value indicates how
 * many good Sync/FollowUp message pairs are received before the
 * re-calculation is performed.
 */
#define XEMACPS_NUM_SYNC_FU_PAIR_CALC_RTC_INCREMENT	2

/* PHY register number and register content mask used for PHY detection.*/
#define PHY_DETECT_REG 					1
#define PHY_DETECT_MASK 				0x1808


/*
 * Maximum buffer length used to store the PTP pakcets. Max buffer length
 * for ethernet packet can never be so high as 1598. But this is done
 * to ensure that start address of each Rx buffer is cache line aligned.
 */
#define XEMACPS_PACKET_LEN				1598

/* Various offsets in the PTP Ethernet packet and masks to extract contents */
#define XEMACPS_MSGTYP_OFFSET				14
#define XEMACPS_MSGTYP_MASK				0x0F
#define XEMACPS_VERSPTP_OFFSET				15
#define XEMACPS_MSGLENGTH_OFFSET			16
#define XEMACPS_FLAGS_OFFSET				20
#define XEMACPS_CORRFIELD_OFFSET			22
#define XEMACPS_PORTIDENTITY_OFFSET			34
#define XEMACPS_SEQID_OFFSET				44
#define XEMACPS_CONTROL_OFFSET				46
#define XEMACPS_LOGMSG_INTERVAL_OFFSET			47
#define XEMACPS_PRECISE_TS_OFFSET			48
#define XEMACPS_CURRUTCOFFSET_OFFSET			58
#define XEMACPS_GMPRI_ONE_OFFSET			61
#define XEMACPS_GM_CLK_QUALITY_OFFSET			62
#define XEMACPS_GMPRI_TWO_OFFSET			66
#define XEMACPS_GM_IDENTITY_OFFSET			67
#define XEMACPS_STEPS_REMOVED_OFFSET			75
#define XEMACPS_TIMESOURCE_OFFSET			77
#define XEMACPS_TLVTYPE_OFFSET				78
#define XEMACPS_LENGTHFIELD_OFFSET			80
#define XEMACPS_PATHSEQ_OFFSET				82
#define XEMACPS_REQPORTID_OFFSET			58

/*
 * The PTP message type, length value, flags value that are
 * populated in different PTP frames
 */
#define XEMACPS_SYNCFRM_MSG_TYPE			0x10
#define XEMACPS_SYNCFRM_LENGTH				0x002C
#define XEMACPS_SYNCFRM_FLAGS_VAL			0x0200
#define XEMACPS_FOLLOWUPFRM_MSG_TYPE			0x18
#define XEMACPS_FOLLOWUPFRM_LENGTH			0x004C
#define XEMACPS_PDELAYREQFRM_LENGTH			54
#define XEMACPS_PDELAYREQFRM_FLAGS_VAL			0x0200
#define XEMACPS_PDELAYREQFRM_MSG_TYPE			0x02
#define XEMACPS_PDELAYRESPFRM_MSG_TYPE			0x03
#define XEMACPS_PDELAYRESPFRM_LENGTH			54
#define XEMACPS_PDELAYRESPFOLLOWUPFRM_MSG_TYPE		0x0A
#define XEMACPS_PDELAYRESPFOLLOWUP_LENGTH		54
#define XEMACPS_ANNOUNCEFRM_MSG_TYPE			0x1B
#define XEMACPS_ANNOUNCEFRM_LENGTH			0x0040
#define XEMACPS_ANNOUNCEFRM_FLAGS_VAL			0x0008

/* The total length of various PTP packets */
#define XEMACPS_ANNOUNCEMSG_TOT_LEN			90
#define XEMACPS_SYNCMSG_TOT_LEN				58
#define XEMACPS_FOLLOWUPMSG_TOT_LEN			90
#define XEMACPS_PDELAYREQMSG_TOT_LEN			68
#define XEMACPS_PDELAYRESPMSG_TOT_LEN			68
#define XEMACPS_PDELAYRESPFOLLOWUP_TOT_LEN		68

/*
 * The bit field information for different PTP packets in the variable
 * PTPSendPacket. This variable controls the sending of PTP packets.
 */
#define SEND_PDELAY_RESP				0x00000001
#define SEND_PDELAY_RESP_FOLLOWUP			0x00000002
#define SEND_PDELAY_REQ					0x00000004
#define SEND_SYNC					0x00000008
#define SEND_FOLLOW_UP					0x00000010

#define NS_PER_SEC 1000000000ULL      /* Nanoseconds per second */
#define FP_MULT    1000ULL

/* Advertisement control register. */
#define ADVERTISE_10HALF	0x0020  /* Try for 10mbps half-duplex  */
#define ADVERTISE_1000XFULL	0x0020  /* Try for 1000BASE-X full-duplex */
#define ADVERTISE_10FULL	0x0040  /* Try for 10mbps full-duplex  */
#define ADVERTISE_1000XHALF	0x0040  /* Try for 1000BASE-X half-duplex */
#define ADVERTISE_100HALF	0x0080  /* Try for 100mbps half-duplex */
#define ADVERTISE_1000XPAUSE	0x0080  /* Try for 1000BASE-X pause    */
#define ADVERTISE_100FULL	0x0100  /* Try for 100mbps full-duplex */
#define ADVERTISE_1000XPSE_ASYM	0x0100  /* Try for 1000BASE-X asym pause */
#define ADVERTISE_100BASE4	0x0200  /* Try for 100mbps 4k packets  */


#define ADVERTISE_100_AND_10	(ADVERTISE_10FULL | ADVERTISE_100FULL | \
				ADVERTISE_10HALF | ADVERTISE_100HALF)
#define ADVERTISE_100		(ADVERTISE_100FULL | ADVERTISE_100HALF)
#define ADVERTISE_10		(ADVERTISE_10FULL | ADVERTISE_10HALF)

#define ADVERTISE_1000		0x0300


// TSU CLock
#define XEMACPS_PTP_TSU_TX_SEC_OFFSET			0x000001E0
#define XEMACPS_PTP_TSU_TX_NSEC_OFFSET			0x000001E4
#define XEMACPS_PTP_TSU_RX_SEC_OFFSET			0x000001E8
#define XEMACPS_PTP_TSU_RX_NSEC_OFFSET 			0x000001EC

#define XEMACPS_PTP_RX_SEC_REG 					0xFF0E01E8
#define XEMACPS_PTP_RX_NSEC_REG					0xFF0E01E4

#define XEMACPS_NWCTRL_OSSM_MASK 				0x01000000
#define XEMACPS_PTP_INT_SYNC_RX_MASK 			0x00080000
#define XEMACPS_PTP_INT_SYNC_TX_MASK 			0x00200000


#define XEMACPS_PTP_TSU_SEC_REG_OFFSET 			0x000001D0
#define XEMACPS_PTP_TSU_NSEC_REG_OFFSET 		0x000001D4

#define XEMACPS_PTP_TSU_INC_OFFSET				0x000001DC
#define XEMACPS_PTP_TSU_ADJUST_OFFSET			0x000001D8

#define PTP_TS_BUFF_SIZE 20

#define ETH_PTP_PositiveTime      ((uint32_t)0x00000000)  /*!< Positive time value */
#define ETH_PTP_NegativeTime      ((uint32_t)0x100000000)  /*!< Negative time value */

/**************************** Type Definitions *******************************/

typedef struct {
	u32 seconds;
	u32 nseconds;
}XEmacPs_Ptp_ts;

struct ptptime_t {
  s32_t tv_sec;
  s32_t tv_nsec;
};


/***************** Macros (Inline Functions) Definitions *********************/

/************************** Function Prototypes ******************************/

void XEmacPs_InitTsu(void);
void XEmacPs_EnableChecksumOffload(xemacpsif_s *xemacpsif);
void XEmacPs_initPtp(xemacpsif_s *xemacpsif);
void XEmacPs_GetRxTimestamp(void);
void XEmacPs_GetTxTimestamp(void);
u32 XEmacPs_TsuCalcClk(u32 Freq);


/*** Prototypes for PTPd Interface */
void ETH_SetPTPTimeStampUpdate(uint32_t Sign, uint32_t SecondValue, uint32_t NanoSecondValue);
void ETH_PTPTime_UpdateOffset(struct ptptime_t * timeoffset);
void ETH_SetPTPTimeStampAddend(uint32_t Value);
void ETH_EnablePTPTimeStampAddend(void);
void ETH_PTPTime_AdjFreq(int32_t Adj);

#ifdef __cplusplus
}
#endif

#endif /* end of protection macro */
