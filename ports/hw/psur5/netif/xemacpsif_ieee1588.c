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

/*
 ************************************************************************************************
 *                                          Defines
 ************************************************************************************************
 */

/*
 ************************************************************************************************
 *                                          GLOBAL VARIABLES
 ************************************************************************************************
 */

u8_t rx_ts_index = 0;
u8_t tx_ts_index = 0;
XEmacPs_Ptp_ts Ptp_Rx_Timestamp[PTP_TS_BUFF_SIZE];
XEmacPs_Ptp_ts Ptp_Tx_Timestamp[PTP_TS_BUFF_SIZE];

/*****************************************************************************/
/**
 * Calculate clock configuration register values for indicated input clock
 *
 * @param	- Freq
 *
 * @return	- nanoseconds per cycle
 *
 * @note		None.
 *
 ******************************************************************************/
u32 XEmacPs_TsuCalcClk(u32 Freq) {
	u32 Period_ns = (NS_PER_SEC) / Freq;

	return Period_ns;
}

/*
 * Write to network_config and dma_config register
 * to enable Rx and Tx Checksum Offloading.
 * LwIP still has to be informed about the crc offload
 * @param netif to be worked on
 */
void XEmacPs_EnableChecksumOffload(xemacpsif_s *xemacpsif) {

	u32 Reg;

	//Read network_config Register
	Reg = XEmacPs_ReadReg(xemacpsif->emacps.Config.BaseAddress,
			XEMACPS_NWCFG_OFFSET);

	// Enable Rx Checksum Offload in network_config reg
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_NWCFG_OFFSET,
			Reg | XEMACPS_NWCFG_RXCHKSUMEN_MASK);

	// Read dma_config register
	Reg = XEmacPs_ReadReg(xemacpsif->emacps.Config.BaseAddress,
			XEMACPS_DMACR_OFFSET);

	// Enable Tx Checksum Offload in dma_config reg
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_DMACR_OFFSET,
			Reg | XEMACPS_DMACR_TCPCKSUM_MASK);

}

/*****************************************************************************/
/**
 * Write Timer Increment in tsu_timer_inc register.
 *
 * @note		None.
 *
 ******************************************************************************/
void XEmacPs_InitTsu(void) {

	u32 NSIncrementVal;
	u32 Freq;

	NSIncrementVal = XEmacPs_TsuCalcClk(
	XPAR_PSU_ETHERNET_3_ENET_TSU_CLK_FREQ_HZ);

	u32 *Addr = (u32 *) 0xFF0E01DC;
	*Addr = NSIncrementVal;

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

	XEmacPs_InitTsu();
	u32 regVal;

	regVal = XEmacPs_ReadReg(xemacpsif->emacps.Config.BaseAddress,
			XEMACPS_NWCTRL_OFFSET);
	/* Enable One Step Sync */
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress,
			XEMACPS_NWCTRL_OFFSET, regVal | XEMACPS_NWCTRL_OSSM_MASK);
	/*Enable PTP Sync Received Interrupt */
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_IER_OFFSET,
			XEMACPS_PTP_INT_SYNC_RX_MASK);
	/* Enable PTP Sync Transmitted Interrupt */
	XEmacPs_WriteReg(xemacpsif->emacps.Config.BaseAddress, XEMACPS_IER_OFFSET,
			XEMACPS_PTP_INT_SYNC_TX_MASK);

}

void XEmacPs_GetRxTimestamp(void) {

	u32 s_val;
	u32 ns_val;

	s_val = XEmacPs_ReadReg(XPAR_XEMACPS_0_BASEADDR, XEMACPS_PTP_TSU_RX_SEC_OFFSET);
	ns_val = XEmacPs_ReadReg(XPAR_XEMACPS_0_BASEADDR, XEMACPS_PTP_TSU_RX_NSEC_OFFSET);

	Ptp_Rx_Timestamp[rx_ts_index].seconds = s_val;
	Ptp_Rx_Timestamp[rx_ts_index].nseconds = ns_val;

	rx_ts_index++;

}

void XEmacPs_GetTxTimestamp(void) {

	u32 s_val;
	u32 ns_val;

	s_val = XEmacPs_ReadReg(XPAR_XEMACPS_0_BASEADDR, XEMACPS_PTP_TSU_TX_SEC_OFFSET);
	ns_val = XEmacPs_ReadReg(XPAR_XEMACPS_0_BASEADDR, XEMACPS_PTP_TSU_TX_NSEC_OFFSET);

	Ptp_Tx_Timestamp[tx_ts_index].seconds = s_val;
	Ptp_Tx_Timestamp[tx_ts_index].nseconds = ns_val;

	tx_ts_index++;

}

void ETH_PTPTime_GetTime(struct ptptime_t* timestamp) {

	timestamp->tv_nsec = XEmacPs_ReadReg(XPAR_XEMACPS_0_BASEADDR,
			XEMACPS_PTP_TSU_NSEC_REG_OFFSET);
	timestamp->tv_sec = XEmacPs_ReadReg(XPAR_XEMACPS_0_BASEADDR,
			XEMACPS_PTP_TSU_SEC_REG_OFFSET);

}

/*******************************************************************************
 * Function Name  : ETH_PTPTimeStampSetTime
 * Description    : Initialize time base
 * Input          : Time with sign
 * Output         : None
 * Return         : None
 *******************************************************************************/
void ETH_PTPTime_SetTime(struct ptptime_t * timestamp) {
	uint32_t Sign;
	uint32_t SecondValue;
	uint32_t NanoSecondValue;
	uint32_t SubSecondValue;

	/* determine sign and correct Second and Nanosecond values */
//	if(timestamp->tv_sec < 0 || (timestamp->tv_sec == 0 && timestamp->tv_nsec < 0))
//	{
//		Sign = ETH_PTP_NegativeTime;
//		SecondValue = -timestamp->tv_sec;
//		NanoSecondValue = -timestamp->tv_nsec;
//	}
//	else
//	{
//		Sign = ETH_PTP_PositiveTime;
//		SecondValue = timestamp->tv_sec;
//		NanoSecondValue = timestamp->tv_nsec;
//	}
	Sign = ETH_PTP_PositiveTime;
	SecondValue = timestamp->tv_sec;
	NanoSecondValue = timestamp->tv_nsec;

	/* convert nanosecond to subseconds */
//	SubSecondValue = ETH_PTPNanoSecond2SubSecond(NanoSecondValue);
	/* Write the offset (positive or negative) in the Time stamp update high and low registers. */
	ETH_SetPTPTimeStampUpdate(Sign, SecondValue, NanoSecondValue);
}

void ETH_SetPTPTimeStampUpdate(uint32_t Sign, uint32_t SecondValue,
		uint32_t NanoSecondValue) {
	/* Set the PTP Time Update High Register */
	XEmacPs_WriteReg(XPAR_XEMACPS_0_BASEADDR, XEMACPS_PTP_TSU_SEC_REG_OFFSET,
			SecondValue);

	/* Set the PTP Time Update Low Register with sign */
//  ETH->PTPTSLUR = Sign | SubSecondValue;
	XEmacPs_WriteReg(XPAR_XEMACPS_0_BASEADDR, XEMACPS_PTP_TSU_NSEC_REG_OFFSET,
			NanoSecondValue);
}

/*
 *******************************************************************************
 * Function Name  : ETH_PTPTimeStampUpdateOffset
 * Description    : Updates time base offset
 * Input          : Time offset with sign
 * Output         : None
 * Return         : None
 *******************************************************************************/
void ETH_PTPTime_UpdateOffset(struct ptptime_t * timeoffset) {
	uint32_t Sign;
	uint32_t SecondValue;
	uint32_t NanoSecondValue;
	uint32_t SubSecondValue;
	uint32_t addend;

	/* determine sign and correct Second and Nanosecond values */
	if (timeoffset->tv_sec < 0
			|| (timeoffset->tv_sec == 0 && timeoffset->tv_nsec < 0)) {
		Sign = ETH_PTP_NegativeTime;
		SecondValue = -timeoffset->tv_sec;
		NanoSecondValue = -timeoffset->tv_nsec;
	} else {
		Sign = ETH_PTP_PositiveTime;
		SecondValue = timeoffset->tv_sec;
		NanoSecondValue = timeoffset->tv_nsec;
	}

	/* TODO */
	/* read old addend register value*/
//	addend = ETH_GetPTPRegister(ETH_PTPTSAR_TSA); // richtiges Register?
	/* Write the offset (positive or negative) in the Time stamp update high and low registers. */
	ETH_SetPTPTimeStampUpdate(Sign, SecondValue, NanoSecondValue);

	/* Write back old addend register value. */
//	ETH_SetPTPTimeStampAddend(addend);
	/* Enable TS Addend */
//	ETH_EnablePTPTimeStampAddend();
}

/**
 * @brief  Updated the PTP block for fine correction with the Time Stamp Addend register value.
 * @param  None
 * @retval None
 */
void ETH_EnablePTPTimeStampAddend(void) {
	//TODO
	/* Enable the PTP block update with the Time Stamp Addend register value */
//  ETH->PTPTSCR |= ETH_PTPTSCR_TSARU;
}

/**
 * @brief  Sets the Time Stamp Addend value.
 * @param  Value: specifies the PTP Time Stamp Addend Register value.
 * @retval None
 */
void ETH_SetPTPTimeStampAddend(uint32_t Value) {
	/* Set the PTP Time Stamp Addend Register */
	XEmacPs_WriteReg(XPAR_XEMACPS_0_BASEADDR, XEMACPS_PTP_TSU_INC_OFFSET, Value);
}

/*******************************************************************************
 * Function Name  : ETH_PTPTimeStampAdjFreq
 * Description    : Updates time stamp addend register
 * Input          : Correction value in thousandth of ppm (Adj*10^9)
 * Output         : None
 * Return         : None
 *******************************************************************************/
void ETH_PTPTime_AdjFreq(int32_t Adj) {
	uint32_t addend;

	Adj = Adj / 10;

	/* calculate the rate by which you want to speed up or slow down the system time
	 increments */

	/* precise */
	/*
	 int64_t addend;
	 addend = Adj;
	 addend *= ADJ_FREQ_BASE_ADDEND;
	 addend /= 1000000000-Adj;
	 addend += ADJ_FREQ_BASE_ADDEND;
	 */

	/* 32bit estimation
	 ADJ_LIMIT = ((1l<<63)/275/ADJ_FREQ_BASE_ADDEND) = 11258181 = 11 258 ppm*/
	if (Adj > 5120000)
		Adj = 5120000;
	if (Adj < -5120000)
		Adj = -5120000;

//	addend = ((((275LL * Adj)>>8) * (ADJ_FREQ_BASE_ADDEND>>24))>>6) + ADJ_FREQ_BASE_ADDEND;

	/* Reprogram the Time stamp addend register with new Rate value and set ETH_TPTSCR */
	ETH_SetPTPTimeStampAddend((uint32_t) addend);
	ETH_EnablePTPTimeStampAddend();
}
