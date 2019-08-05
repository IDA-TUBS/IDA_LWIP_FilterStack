/*
 ============================================================================
 Name        : xemacif_ieee1588.c
 Author      : Phil Hertha
 Version     :
 Copyright   : Copyright belongs to the authors
 Description : PTP IEEE 1588 Functionality in Xilinx EMacPS
 ============================================================================
 */
#include "stdio.h"
#include "xparameters.h"
#include "xparameters_ps.h"	/* defines XPAR values */
#include "xil_types.h"
#include "xemacps.h"		/* defines XEmacPs API */
#include "netif/xemacps_ieee1588.h"

#include "netif/xemacpsif.h"
#include "netif/xadapter.h"

// Includes for use with ptpd
#include "ptpd.h"

#if XPAR_EMACPS_TSU_ENABLE
/*
 ************************************************************************************************
 *                                          Defines
 ************************************************************************************************
 */
#if defined (ARMR5)
#define XPAR_XEMACPS_BASEADDR XPAR_PSU_ETHERNET_3_BASEADDR
#define ENET_TSU_CLK_FREQ_HZ XPAR_PSU_ETHERNET_3_ENET_TSU_CLK_FREQ_HZ
#else
#define XPAR_XEMACPS_BASEADDR XPAR_XEMACPS_0_BASEADDR
#define ENET_TSU_CLK_FREQ_HZ XPAR_PS7_ETHERNET_0_ENET_TSU_CLK_FREQ_HZ
#endif

/*
 ************************************************************************************************
 *                                          GLOBAL VARIABLES
 ************************************************************************************************
 */


/*
 * PTP Timestamp from Rx ptp capture register
 */
uint32_t Ptp_RxTimeStampSeconds;
uint32_t Ptp_RxTimeStampNSeconds;

/*
 * PTP Timestamp from Tx ptp capture register
 */
uint32_t Ptp_TxTimeStampSeconds;
uint32_t Ptp_TxTimeStampNSeconds;

/*
 * Semaphore to inform the ptp interface about
 * new captures
 */
sys_sem_t sem_tx_ptp_available;

/*
 * Initial TSU Increment
 */
XEmacPs_Tsu_incr Ptp_TSU_base_incr;


extern PtpClock ptpClock;

/*****************************************************************************/
/**
 * Write to network_config and dma_config register
 * to enable Rx and Tx Checksum Offloading.
 * LwIP still has to be informed about the crc offload
 *
 * @param xemacpsif_s *xemacpsif instance to be worked on
 *
 * @note		None.
 *
 ******************************************************************************/
void XEmacPs_EnableChecksumOffload(xemacpsif_s *xemacpsif) {

	u32 Reg;

	//Read network_config Register
	Reg = XEmacPs_ReadReg(xemacpsif->emacps.Config.BaseAddress,XEMACPS_NWCFG_OFFSET);

	// Enable Rx Checksum Offload in network_config reg
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_NWCFG_OFFSET, Reg | XEMACPS_NWCFG_RXCHKSUMEN_MASK);

	// Read dma_config register
	Reg = XEmacPs_ReadReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_DMACR_OFFSET);

	// Enable Tx Checksum Offload in dma_config reg
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_DMACR_OFFSET, Reg | XEMACPS_DMACR_TCPCKSUM_MASK);

}

/*****************************************************************************/
/**
 * Write Timer Increment in tsu_timer_inc register.
 *
 * @note		None.
 *
 ******************************************************************************/
void XEmacPs_InitTsu(void) {

	u64 temp;

	Ptp_TSU_base_incr.nanoseconds = NS_PER_SEC / ENET_TSU_CLK_FREQ_HZ;


	temp = (NS_PER_SEC - Ptp_TSU_base_incr.nanoseconds * ENET_TSU_CLK_FREQ_HZ);
	temp *= (1 << XEMACPS_PTP_TSU_SUB_NS_INCR_SIZE);
	Ptp_TSU_base_incr.subnanoseconds = temp / ENET_TSU_CLK_FREQ_HZ;

	XEmacPs_WriteTsuIncr(Ptp_TSU_base_incr.nanoseconds, Ptp_TSU_base_incr.subnanoseconds);

}

/*****************************************************************************/
/**
 * Calculate clock configuration register values for indicated input clock
 *
 * @param	IntanzPtr to work on.
 *
 * @return	None.
 *
 * @note		None.
 *
 ******************************************************************************/
void XEmacPs_initPtp(xemacpsif_s *xemacpsif) {

	u32 regVal;


	regVal = XEmacPs_ReadReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_NWCTRL_OFFSET);
#ifdef TSU_OSSM
	/* Enable One Step Sync Mode */
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_NWCTRL_OFFSET, regVal | XEMACPS_NWCTRL_OSSM_MASK);
#endif
	/*Enable PTP Sync Received Interrupt */
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_IER_OFFSET, XEMACPS_IXR_PTPSRX_MASK);

	/*Enable PTP Delay_req Received Interrupt */
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_IER_OFFSET, XEMACPS_IXR_PTPDRRX_MASK);

	/* Enable PTP Sync Transmitted Interrupt */
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_IER_OFFSET, XEMACPS_IXR_PTPPSRX_MASK);

	/*Enable PTP Delay_req Transmitted Interrupt */
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_IER_OFFSET, XEMACPS_IXR_PTPDRTX_MASK);

	/* Enable PTP TSU Compare Interrupt */
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_IER_OFFSET, XEMACPS_IXR_PTP_CMP_MASK);

#if XPAR_EMACPS_PPS_IRQ_ENABLE
	/* Enable PTP Pulse Per Second IRQ */
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_IER_OFFSET, XEMACPS_IXR_PTP_PPS_MASK);
#endif

	XEmacPs_InitTsu();
}


/*****************************************************************************/
/**
 * Reads tsu_ptp_rx_sec and tsu_ptp_rx_nsec Registers from GEM3.
 * An Interrupt is raised when an PTP Sync Frame Event Crosses the MII Boundaries.
 * The ISR calls this function to store the received timestamps in a ts buffer.
 *
 * @param	None.
 *
 * @return	None.
 *
 * @note	For use with ptp slave.
 *
 ******************************************************************************/
void XEmacPs_GetRxTimestamp(void) {
	Ptp_RxTimeStampSeconds = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_RX_SEC_OFFSET);
	Ptp_RxTimeStampNSeconds = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_RX_NSEC_OFFSET);
}


/*****************************************************************************/
/**
 * Reads tsu_ptp_rx_sec and tsu_ptp_rx_nsec Registers from GEM3.
 * An Interrupt is raised when an PTP Delay Req is received
 * and crosses the MII Boundaries.
 * The ISR calls this function to store the received timestamps in the ptpClock instance.
 * It will be processed in ptpd handleDelayReq()
 * TODO: Can we use this TS, oder do we have to normalize it?
 *
 * @param	None.
 *
 * @return	None.
 *
 * @note	For use with ptp master.
 *
 ******************************************************************************/
void XEmacPs_GetDelayReqRxTimestamp(void) {
	TimeInternal ptpTimestamp;
	ptpTimestamp.seconds = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_RX_SEC_OFFSET);
	ptpTimestamp.nanoseconds = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_RX_NSEC_OFFSET);


	ptpClock.timestamp_delayReqRecieve = ptpTimestamp;
}


/*****************************************************************************/
/**
 * Reads tsu_ptp_rx_sec and tsu_ptp_rx_nsec Registers from GEM3.
 * An Interrupt is raised when an PTP Delay Req is transmitted and
 * crosses the MII Boundaries.
 * The ISR calls this function to store the received timestamps in the ptpClock instance.
 * It will be processed in ptpd handleDelayReq()
 * TODO: Can we use this TS, oder do we have to normalize it?
 *
 * @param	None.
 *
 * @return	None.
 *
 * @note	For use with ptp slave.
 *
 ******************************************************************************/
void XEmacPs_GetDelayReqTxTimestamp(void) {
	TimeInternal ptpTimestamp;

	ptpTimestamp.seconds = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_TX_SEC_OFFSET);
	ptpTimestamp.nanoseconds = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_TX_NSEC_OFFSET);

	if (ptpTimestamp.seconds != 0) {
		addTime(&ptpTimestamp, &ptpTimestamp, &ptpClock.outboundLatency);
		ptpClock.timestamp_delayReqSend = ptpTimestamp;
	}

}


/*****************************************************************************/
/**
*	Read the PTP Tx Capture register and write them to the Tx Timestamp buffer
*
* @param N/A
*
* @return
*
* @note For use with ptp master
*
******************************************************************************/
void XEmacPs_GetTxTimestamp(void) {

	Ptp_TxTimeStampSeconds = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_TX_SEC_OFFSET);
	Ptp_TxTimeStampNSeconds = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_TX_NSEC_OFFSET);
	sys_sem_signal(&sem_tx_ptp_available);

}


/*****************************************************************************/
/**
*	Read the current timer values from tsu_sec_register
 * 	and tsu_nsec_register
*
* @param timestamp in which the values are written
*
* @return N/A
*
******************************************************************************/
void ETH_PTPTime_GetTime(struct ptptime_t* timestamp) {

	timestamp->tv_sec = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_SEC_REG_OFFSET);

	timestamp->tv_nsec = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_NSEC_REG_OFFSET);

	if (XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_SEC_REG_OFFSET) > timestamp->tv_sec) {
		timestamp->tv_sec = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_SEC_REG_OFFSET);
	}
}

/*****************************************************************************/
/**
*	Initialize time base
*
* @param Sign wether the Time is pos or neg.
* 		 SecondsValue Seconds to be writen to the reg
* 		 NanoSecondValue	Nseconds to be writen
*
* @return N/A
*
******************************************************************************/
void ETH_PTPTime_SetTime(struct ptptime_t * timestamp) {

	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_SEC_REG_OFFSET, timestamp->tv_sec);
	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_NSEC_REG_OFFSET, timestamp->tv_nsec);
}

/*****************************************************************************/
/**
*	Initialize time base
*
* @param Sign wether the Time is pos or neg.
* 		 SecondsValue Seconds to be writen to the reg
* 		 NanoSecondValue	Nseconds to be writen
*
* @return N/A
*
******************************************************************************/
void ETH_PTPTime_SetCompare(uint32_t sec, uint32_t nsec) {

	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_TSU_SEC_CMP_OFFSET, sec);
	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_TSU_NSEC_CMP_OFFSET, (nsec >> 8));
}

/*****************************************************************************/
/**
* Frequenzy Adjustment by adjusting the nanoseconds and subnanoseconds inrements:
* The base(!) nano- and subnano increments are concatenated in one fractional word.
* The delta is computed as follows.
*
* 		delta = ppb * word / 10^-9 (Nanoseconds per second)
*
* This delta is added/subtracted from the word and the nanoseconds/subnanoseconds
* portion of this word is writen to the inrement registers
*
* @param Adj is the correction value in thousands if ppm (Adj *10^-9)
*
* @return N/A
*
* @note
*
******************************************************************************/
void ETH_PTPTime_AdjFreq(int32_t Adj) {
	XEmacPs_Tsu_incr incr;
	BOOLEAN neg_adj = 0;
	u64 temp;
	u32 word;
	s32 ppb = Adj;							// Part per Billion (ns) = adj

	if (ppb < 0) {							// Check if error is positive or negative
		neg_adj = TRUE;
		ppb = -ppb;
	}

	incr.nanoseconds = Ptp_TSU_base_incr.nanoseconds;
	incr.subnanoseconds = Ptp_TSU_base_incr.subnanoseconds;

	/* scaling: ns(8bit) | fractions(24bit) */
	word = ((u64)incr.nanoseconds << 24) + incr.subnanoseconds;
	temp = (u64)ppb * word;
	/* Divide with rounding, equivalent to floating dividing:
	 * (temp / USEC_PER_SEC) + 0.5
	 */
	temp = temp / (u64)NS_PER_SEC;
	temp = neg_adj ? (word - temp) : (word + temp);

	/* write the calculated nanoseconds and subnanoseconds increment in the register */
	incr.nanoseconds = (temp >> XEMACPS_PTP_TSU_SUB_NS_INCR_SIZE) & ((1 << XEMACPS_PTP_TSU_NS_INCR_SIZE) - 1);
	incr.subnanoseconds = temp & ((1 << XEMACPS_PTP_TSU_SUB_NS_INCR_SIZE) - 1);

	XEmacPs_WriteTsuIncr(incr.nanoseconds, incr.subnanoseconds);
}

/*****************************************************************************/
/**
* Offset Adjustment
*
* @param Adj is the correction value in ppb (ns)
*
* @return N/A
*
* @note
*
******************************************************************************/
void ETH_PTPTime_AdjOffset(int32_t Adj) {
	u32 offset;							// Part per Billion (ns) = adj

	if (Adj < 0) {						// Check if error is positive or negative
		offset = -Adj;
		offset |= (1 << 31);
	} else {
		offset = Adj;
	}

	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_ADJUST_OFFSET, offset);
}

/*****************************************************************************/
/**
* Write the tsu_timer_incr and tsu_timer_incr_sub_nsec.
* These are the values by which the timer register will be imcremented
* each clock cycle.
*
* @param ns is the nanosecond value to be writen
* @param subns is the subnanoseconds value
*
* @return N/A
*
* @note tsu_timer_incr register must be written after the tsu_timer_incr_sub_ns register
	 and the write operation will cause the value written to the tsu_timer_incr_sub_ns
	 register to take effect.
*
******************************************************************************/
void XEmacPs_WriteTsuIncr(u32 ns, u32 subns)
{
	u32 sub_ns_reg;

	/*
	 * subnanoseconds are divided into msb and lsb.
	 * lsb [31:24], msb [15:0]
	 */
	sub_ns_reg = ((subns  & XEMACPS_PTP_TSU_SUB_NS_LSB_MASK) << XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_OFFSET) | ((subns  & XEMACPS_PTP_TSU_SUB_NS_MSB_MASK) >> XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_SIZE);

	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_SUB_INC_OFFSET, sub_ns_reg);
	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_INC_OFFSET, ns);
}

/*****************************************************************************/
/**
* Read the current Inrements
*
* @return XEmacPs_Tsu_incr incr is the increment, including nanoseconds and subns
*
******************************************************************************/
XEmacPs_Tsu_incr XEmacPs_ReadTsuIncr(void)
{
	u32 sub_ns_reg;
	u32 ns;
	XEmacPs_Tsu_incr incr;


	sub_ns_reg = XEmacPs_ReadReg((u32 )XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_SUB_INC_OFFSET);
	incr.nanoseconds = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_INC_OFFSET);

	/* RegBit[15:0] = Subns[23:8]; RegBit[31:24] = Subns[7:0] */
	sub_ns_reg = ((sub_ns_reg & ((1 << XEMACPS_PTP_TSU_SUB_NS_INCR_MSB_SIZE) - 1)) << XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_SIZE)
			| ((sub_ns_reg & ~((1 << XEMACPS_PTP_TSU_SUB_NS_INCR_MSB_SIZE) - 1)) >> (XEMACPS_PTP_TSU_SUB_NS_INCR_MSB_SIZE + XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_SIZE));

	incr.subnanoseconds = sub_ns_reg;

	return incr;
}

/*****************************************************************************/
/**
* Get the current PTP Timestamp from the capture register
*
* @param time_s nanoseconds portion from timestamp
* @param time_ns subnanoseconds portion
* @param receive is the direction, true for receive
*
*
******************************************************************************/
void ETH_PTP_GetTimestamp(int32_t *time_s, int32_t *time_ns, BOOLEAN receive)
{
	if(!receive){
		sys_sem_wait(&sem_tx_ptp_available);
		*time_s = Ptp_TxTimeStampSeconds;
		*time_ns = Ptp_TxTimeStampNSeconds;
	} else {
		*time_s = Ptp_RxTimeStampSeconds;
		*time_ns = Ptp_RxTimeStampNSeconds;

	}
}
#endif
