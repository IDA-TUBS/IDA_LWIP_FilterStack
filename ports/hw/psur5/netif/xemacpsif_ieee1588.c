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


XEmacPs_Ptp_ts_buff Ptp_Rx_Timestamp;
XEmacPs_Ptp_ts_buff Ptp_Tx_Timestamp;

struct ptptime_t PTP_BUFF;

sys_sem_t sem_tx_ptp_available;


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

	u32 NSIncrementVal, SubNSIncrementVal;
	u32 Freq;
	u32 temp;

	NSIncrementVal = XEmacPs_TsuCalcClk(
			ENET_TSU_CLK_FREQ_HZ);


	temp = (NS_PER_SEC - NSIncrementVal * ENET_TSU_CLK_FREQ_HZ);
	temp *= (1 << XEMACPS_PTP_TSU_SUB_NS_INCR_SIZE);
	SubNSIncrementVal = temp / ENET_TSU_CLK_FREQ_HZ;

//	u32 *Addr = (u32 *) 0xFF0E01DC;
//	*Addr = NSIncrementVal;
//
//	Addr = (u32 *) 0xFF0E01BC;
//	*Addr = SubNSIncrementVal;

	XEmacPs_WriteTsuIncr(NSIncrementVal, SubNSIncrementVal);

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

	Ptp_Rx_Timestamp.tail = 0;
	Ptp_Tx_Timestamp.tail = 0;

	Ptp_Rx_Timestamp.head = 0;
	Ptp_Tx_Timestamp.head = 0;

	Ptp_Rx_Timestamp.len = 0;
	Ptp_Tx_Timestamp.len = 0;

	XEmacPs_InitTsu();

}


/*****************************************************************************/
/**
 * Reads tsu_ptp_rx_sec and tsu_ptp_rx_nsec Registers from GEM3.
 * An Interrupt is raised when an PTP Event Crosses the MII Boundaries.
 * The ISR calls this funktion to store the received timestamps in a ringbuffer.
 *
 * @param	None.
 *
 * @return	None.
 *
 * @note	None.
 *
 ******************************************************************************/
void XEmacPs_GetRxTimestamp(void) {

	u32 s_val;
	u32 ns_val;
	u8 head;
	OS_CPU_SR cpu_sr;

	s_val = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_RX_SEC_OFFSET);
	ns_val = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_RX_NSEC_OFFSET);


	OS_ENTER_CRITICAL();
	head = Ptp_Rx_Timestamp.head;
	Ptp_Rx_Timestamp.seconds[head] = s_val;
	Ptp_Rx_Timestamp.nseconds[head] = ns_val;
	head = (head + 1) % XEMACPS_PTP_TS_BUFF_SIZE;
	Ptp_Rx_Timestamp.head = head;
	Ptp_Rx_Timestamp.len++;
	OS_EXIT_CRITICAL();


}

void XEmacPs_GetTxTimestamp(void) {

	u32 s_val;
	u32 ns_val;
	u8 head;

	OS_CPU_SR cpu_sr;

	s_val = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_TX_SEC_OFFSET);
	ns_val = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_TX_NSEC_OFFSET);

	OS_ENTER_CRITICAL();
	head = Ptp_Tx_Timestamp.head;
	Ptp_Tx_Timestamp.seconds[head] = s_val;
	Ptp_Tx_Timestamp.nseconds[head] = ns_val;
	head = (head + 1) % XEMACPS_PTP_TS_BUFF_SIZE;
	Ptp_Tx_Timestamp.head = head;
	OS_EXIT_CRITICAL();

	sys_sem_signal(&sem_tx_ptp_available);

}
/*******************************************************************************
 * Function Name  : ETH_PTPTime_GetTime
 * Description    : Read the current timer values from tsu_sec_register
 * 					and tsu_nsec_register
 * Input          : timestamp in which the values are written
 * Output         : None
 * Return         : None
 *******************************************************************************/
void ETH_PTPTime_GetTime(struct ptptime_t* timestamp) {

	timestamp->tv_sec = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR,
				XEMACPS_PTP_TSU_SEC_REG_OFFSET);

	timestamp->tv_nsec = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR,
			XEMACPS_PTP_TSU_NSEC_REG_OFFSET);

	if (XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR,
			XEMACPS_PTP_TSU_SEC_REG_OFFSET) > timestamp->tv_sec) {
		timestamp->tv_sec = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR,
						XEMACPS_PTP_TSU_SEC_REG_OFFSET);
	}


}

//u32_t timeSec[10];
//u32_t timeNsec[10];
//u8 timeIndex = 0;
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
	struct ptptime_t *currentTime;

//	if (timeIndex < 10) {
//		timeSec[timeIndex] = timestamp->tv_sec;
//		timeNsec[timeIndex] = timestamp->tv_nsec;
//		timeIndex++;
//	} else
//		asm volatile ("nop");


	/* determine sign and correct Second and Nanosecond values */
	if(timestamp->tv_sec < 0 || (timestamp->tv_sec == 0 && timestamp->tv_nsec < 0))
	{
		Sign = ETH_PTP_NegativeTime;
		SecondValue = -timestamp->tv_sec;
		NanoSecondValue = -timestamp->tv_nsec;
	}
	else
	{
		Sign = ETH_PTP_PositiveTime;
		SecondValue = timestamp->tv_sec;
		NanoSecondValue = timestamp->tv_nsec;
	}

	ETH_SetPTPTimeStampUpdate(Sign, SecondValue, NanoSecondValue);
}

/*
 *******************************************************************************
 * Function Name  : ETH_SetPTPTimeStampUpdate
 * Description    : Overwrites the TSU Timer with values calculated by PTPd
 * Input          : Sign wether the Time is pos or neg.
 * 					Seconds to be writen to the reg
 * 					Nseconds to be writen
 * Output         : None
 * Return         : None
 *******************************************************************************/
void ETH_SetPTPTimeStampUpdate(uint32_t Sign, uint32_t SecondValue,
		uint32_t NanoSecondValue) {

	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_SEC_REG_OFFSET,
			SecondValue);

	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_NSEC_REG_OFFSET,
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

///**
// * @brief  Updated the PTP block for fine correction with the Time Stamp Addend register value.
// * @param  None
// * @retval None
// */
//void ETH_EnablePTPTimeStampAddend(void) {
//	/* Enable the PTP block update with the Time Stamp Addend register value */
////  ETH->PTPTSCR |= ETH_PTPTSCR_TSARU;
//}

/**
 * @brief  Sets the Time Stamp Addend value.
 * @param  Value: specifies the PTP Time Stamp Addend Register value.
 * @retval None
 */
void ETH_SetPTPTimeStampAddend(uint32_t Value) {
	/* Set the PTP Time Stamp Addend Register */
	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_INC_OFFSET, Value);
}



/*******************************************************************************
 * Function Name  : ETH_PTPTimeStampAdjFreq
 * Description    : Updates time stamp addend register
 * Input          : Correction value in thousandth of ppm (Adj*10^9)
 * Output         : None
 * Return         : None
 *******************************************************************************/
uint32_t addendArray[100];
uint32_t subaddednArray[100];
uint8_t addendIndex = 0;
void ETH_PTPTime_AdjFreq(int32_t Adj) {

	u32 addend = 0;
	u32 addendsub = 0;;
//	u32 temp = 0;
//	u32 rem = 0;
	XEmacPs_Tsu_incr incr;
	u32 *incr_ns = 0;
	u32 *incr_subns = 0;
//	s32 ppb_pot = 1000000000;
//	u32 adjsub = 0;
	BOOLEAN neg_adj = 0;
//	u32 clk_rate = ENET_TSU_CLK_FREQ_HZ;
//	u32 diffsub = 0;
	u32 subnsreg = 0;
	u64 period, temp;

	if (Adj != 0)
		asm volatile ("nop");


	s32 ppb = Adj;						// Part per Billion (ns) = adj
	//* ppb_pot;

	if (ppb < 0) {							// Check if error is positive or negative
		neg_adj = TRUE;
		ppb = -ppb;
	}

	incr = XEmacPs_ReadTsuIncr();			// Get current addend and sub addend

	/* scaling */
	period = ((u64) incr.nanoseconds << XEMACPS_PTP_TSU_SUB_NS_INCR_SIZE) + incr.subnanoseconds;
	temp = (u64)ppb * period;
	/* Divide with rounding, equivalent to floating dividing:
	 * (temp / NSEC_PER_SEC) + 0.5
	 */
	temp = (temp + (NS_PER_SEC >> 1))/ NS_PER_SEC;

	period = neg_adj ? (period - temp) : (period + temp);

	incr.nanoseconds = (period >> XEMACPS_PTP_TSU_SUB_NS_INCR_SIZE) & ((1 << GEM_SUBNSINCH_SHFT) - 1);
	incr.subnanoseconds = period & ((1 << XEMACPS_PTP_TSU_SUB_NS_INCR_SIZE) - 1);

	addend = incr.nanoseconds;
	subnsreg = ((incr.subnanoseconds  & GEM_SUBNSINCL_MASK) << GEM_SUBNSINCL_SHFT)
			| ((incr.subnanoseconds  & GEM_SUBNSINCH_MASK) >> GEM_SUBNSINCH_SHFT);

	addendArray[addendIndex] = addend;
	subaddednArray[addendIndex] = incr.subnanoseconds;
	addendIndex = (addendIndex + 1) % 100;

	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_SUB_INC_OFFSET, subnsreg);
	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_INC_OFFSET, addend);
}


void XEmacPs_WriteTsuIncr(u32 ns, u32 subns)
{
	u32 sub_ns_reg;
	u32 ns_reg;

	sub_ns_reg = XEMACPS_SHIFT__LEAST_SUB_NS(subns)
			| (((subns & ~((1 << XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_SIZE) - 1)) >> XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_SIZE));

	/* tsu_timer_incr register must be written after the tsu_timer_incr_sub_ns register
	 * and the write operation will cause the value written to the tsu_timer_incr_sub_ns
	 * register to take effect.
	 */
//	XEmacPs_WriteReg((u32 )XPAR_XEMACPS_BASEADDR, XEMACPS_SUB_NS_INCR_OFFSET, sub_ns_reg);
//	XEmacPs_WriteReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_INC_OFFSET, ns_reg);

	/*
	 * Write NS Increment Value
	 */
	u32 *Addr = (u32 *) 0xFF0E01DC;
	*Addr = ns;

	/*
	 * Write SubNS Increment Value
	 */
	Addr = (u32 *) 0xFF0E01BC;
	*Addr = sub_ns_reg;

}


XEmacPs_Tsu_incr XEmacPs_ReadTsuIncr(void)
{
	u32 sub_ns_reg;
	u32 ns;
	XEmacPs_Tsu_incr incr;


	sub_ns_reg = XEmacPs_ReadReg((u32 )XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_SUB_NS_INCR_OFFSET);
	incr.nanoseconds = XEmacPs_ReadReg(XPAR_XEMACPS_BASEADDR, XEMACPS_PTP_TSU_INC_OFFSET);

	/* RegBit[15:0] = Subns[23:8]; RegBit[31:24] = Subns[7:0] */
	sub_ns_reg = ((sub_ns_reg & ((1 << XEMACPS_PTP_TSU_SUB_NS_INCR_MSB_SIZE) - 1)) << XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_SIZE)
			| ((sub_ns_reg & ~((1 << XEMACPS_PTP_TSU_SUB_NS_INCR_MSB_SIZE) - 1)) >> (XEMACPS_PTP_TSU_SUB_NS_INCR_MSB_SIZE + XEMACPS_PTP_TSU_SUB_NS_INCR_LSB_SIZE));

	incr.subnanoseconds = sub_ns_reg;

	return incr;
}


void ETH_PTP_GetTimestamp(int32_t *time_s, int32_t *time_ns, BOOLEAN receive)
{
	EthPacketTimestamp temp;
	struct ptptime_t timestamp;
	struct ptptime_t txtimestamp;
	u8 index;
	OS_CPU_SR cpu_sr;

	/* nach ptpd-Vorgabe wird hier der Timestamp geholt:
	 * Timestamp sieht sehr groß aus, aber das System funktioniert und der Slave bleibt soaweit kalibriert
	 */
//	ETH_PTPTime_GetTime(&timestamp);


	if(!receive){

		sys_sem_wait(&sem_tx_ptp_available);

		*time_s = txtimestamp.tv_sec;
		*time_ns = txtimestamp.tv_nsec;
	} else {
		OS_ENTER_CRITICAL();

		if (Ptp_Rx_Timestamp.len > 0) {
			index = Ptp_Rx_Timestamp.tail;
			*time_s = Ptp_Rx_Timestamp.seconds[index];
			*time_ns = Ptp_Rx_Timestamp.nseconds[index];
			index = (index + 1) % XEMACPS_PTP_TS_BUFF_SIZE;
			Ptp_Rx_Timestamp.tail = index;
		} else
			asm  volatile ("nop");

		OS_EXIT_CRITICAL();


	}

//	for (int i = 0; i < XEMACPS_PTP_TS_BUFF_SIZE; i++){
//		if (receive){
//			temp = Ptp_Rx_Timestamp[i];
//			if(p == temp.buf){
//				/* Timestamp mit dem aus Ringbuffer (scheint ungenauer zu sein) überschreiben & übergeben */
//				*time_s = temp.timestamp_1.tv_sec;
//				*time_ns = temp.timestamp_1.tv_nsec;
//				return;
//			}
//		}
//	}




}
#endif
