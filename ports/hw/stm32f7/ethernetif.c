/**
  ******************************************************************************
  * @file    LwIP/LwIP_HTTP_Server_Netconn_RTOS/Src/ethernetif.c
  * @author  MCD Application Team
  * @version V1.2.0
  * @date    30-December-2016
  * @brief   This file implements Ethernet network interface drivers for lwIP
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics International N.V. 
  * All rights reserved.</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without 
  * modification, are permitted, provided that the following conditions are met:
  *
  * 1. Redistribution of source code must retain the above copyright notice, 
  *    this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright notice,
  *    this list of conditions and the following disclaimer in the documentation
  *    and/or other materials provided with the distribution.
  * 3. Neither the name of STMicroelectronics nor the names of other 
  *    contributors to this software may be used to endorse or promote products 
  *    derived from this software without specific written permission.
  * 4. This software, including modifications and/or derivative works of this 
  *    software, must execute solely and exclusively on microcontroller or
  *    microprocessor devices manufactured by or for STMicroelectronics.
  * 5. Redistribution and use of this software other than as permitted under 
  *    this license is void and will automatically terminate your rights under 
  *    this license. 
  *
  * THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" 
  * AND ANY EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT 
  * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A 
  * PARTICULAR PURPOSE AND NON-INFRINGEMENT OF THIRD PARTY INTELLECTUAL PROPERTY
  * RIGHTS ARE DISCLAIMED TO THE FULLEST EXTENT PERMITTED BY LAW. IN NO EVENT 
  * SHALL STMICROELECTRONICS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
  * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
  * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, 
  * OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF 
  * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
  * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
  * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f7xx_hal.h"
#include "stm32f7xx_hal_eth.h"

#include "lwip/opt.h"
#include "netif/etharp.h"
#include "ethernetif.h"
#include <string.h>

#include "lwip/timeouts.h"
#include "netif/ethernet.h"

#include "tb.h"

//#include "ucos_ii.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* The time to block waiting for input. */
#define TIME_WAITING_FOR_INPUT                 ( 0 )
/* Stack size of the interface thread */
#define ETH_INPUT_TASK_STACK_SIZE				512
#define ETH_INPUT_TASK_PRIO						20
#define ETH_INPUT_TASK_NAME						"Eth Input Task"

#define PTP_MASTER_TASK_STK_SIZE				50
#define PTP_BLOCK_NUM							50

OS_STK eth_input_task_stk[ETH_INPUT_TASK_STACK_SIZE];

typedef struct PTPMessageQueueStruct{
	uint8_t type;
	uint64_t timestamp;
} ptpmsg_t;

/* Define those to better describe your network interface. */
#define IFNAME0 's'
#define IFNAME1 't'

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/*
  @Note: The DMARxDscrTab and DMATxDscrTab must be declared in a non cacheable memory region
         In this example they are declared in the first 256 Byte of SRAM1 memory, so this
         memory region is configured by MPU as a device memory (please refer to MPU_Config() in main.c).

         In this example the ETH buffers are located in the SRAM2 memory, 
         since the data cache is enabled, so cache maintenance operations are mandatory.
 */
#if defined ( __CC_ARM   )
ETH_DMADescTypeDef  DMARxDscrTab[ETH_RXBUFNB] __attribute__((at(0x20010000)));/* Ethernet Rx MA Descriptor */

ETH_DMADescTypeDef  DMATxDscrTab[ETH_TXBUFNB] __attribute__((at(0x20010080)));/* Ethernet Tx DMA Descriptor */

uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __attribute__((at(0x2004C000))); /* Ethernet Receive Buffer */

uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __attribute__((at(0x2004D7D0))); /* Ethernet Transmit Buffer */

#elif defined ( __ICCARM__ ) /*!< IAR Compiler */
  #pragma data_alignment=4 

#pragma location=0x20010000
__no_init ETH_DMADescTypeDef  DMARxDscrTab[ETH_RXBUFNB];/* Ethernet Rx MA Descriptor */

#pragma location=0x20010080
__no_init ETH_DMADescTypeDef  DMATxDscrTab[ETH_TXBUFNB];/* Ethernet Tx DMA Descriptor */

#pragma location=0x2004C000
__no_init uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE]; /* Ethernet Receive Buffer */

#pragma location=0x2004D7D0
__no_init uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE]; /* Ethernet Transmit Buffer */

#elif defined ( __GNUC__ ) /*!< GNU Compiler */

ETH_DMADescTypeDef  DMARxDscrTab[ETH_RXBUFNB] __attribute__((section(".RxDecripSection")));/* Ethernet Rx MA Descriptor */

ETH_DMADescTypeDef  DMATxDscrTab[ETH_TXBUFNB] __attribute__((section(".TxDescripSection")));/* Ethernet Tx DMA Descriptor */

uint8_t Rx_Buff[ETH_RXBUFNB][ETH_RX_BUF_SIZE] __attribute__((section(".RxarraySection"))); /* Ethernet Receive Buffer */

uint8_t Tx_Buff[ETH_TXBUFNB][ETH_TX_BUF_SIZE] __attribute__((section(".TxarraySection"))); /* Ethernet Transmit Buffer */

#endif

/* Semaphore to signal incoming packets */
OS_EVENT *newEthPacketSem = NULL;

/* Global Ethernet handle*/
ETH_HandleTypeDef EthHandle;


uint8_t triggerTxTimestamp;
uint64_t rxTimestamp, txTimestamp;
__IO ETH_DMADescTypeDef *PTPTxDesc;
uint32_t ptpRxTimeLow = 0, ptpRxTimeHigh = 0, ptpTxTimeLow = 0, ptpTxTimeHigh = 0;

EthPacketTimestamp TxEthPacketRingBuf[ETH_PTP_TIMESTAMP_BUF_SIZE];
EthPacketTimestamp RxEthPacketRingBuf[ETH_PTP_TIMESTAMP_BUF_SIZE];
uint8_t TxRingBufIndex;
uint8_t RxRingBufIndex;

/* Private function prototypes -----------------------------------------------*/

static void ETHInputTask( void * argument );
static void ETH_PTPStart();
static void PTP_Save_TS_Task(void* p_arg);
void notify_Counter_Task(uint8_t type, uint64_t timestamp);

/**
  * @brief  Initialize the PTP Time Stamp
  * @param  None
  * @retval None
  */
void ETH_InitializePTPTimeStamp(void)
{
  /* Initialize the PTP Time Stamp */
  ETH->PTPTSCR |= ETH_PTPTSCR_TSSTI;
}

uint32_t ETH_PTPSubSecond2NanoSecond(uint32_t SubSecondValue)
{
  uint64_t val = SubSecondValue * 1000000000ll;
  val >>=31;
  return val;
}


uint32_t ETH_PTPNanoSecond2SubSecond(uint32_t SubSecondValue)
{
  uint64_t val = SubSecondValue * 0x80000000ll;
  val /= 1000000000;
  return val;
}


void ETH_PTPTime_GetTime(struct ptptime_t* timestamp)
{
  timestamp->tv_nsec = ETH_PTPSubSecond2NanoSecond(ETH->PTPTSLR);
  timestamp->tv_sec = ETH->PTPTSHR;

}

struct ptptime_t txtimestamp;

void ETH_PTP_GetTimestamp(struct pbuf* p, int32_t *time_s, int32_t *time_ns, BOOLEAN receive)
{
	EthPacketTimestamp temp;
	struct ptptime_t timestamp;

	/* nach ptpd-Vorgabe wird hier der Timestamp geholt:
	 * Timestamp sieht sehr groß aus, aber das System funktioniert und der Slave bleibt soaweit kalibriert
	 */
	ETH_PTPTime_GetTime(&timestamp);


	if(!receive){
		*time_s = txtimestamp.tv_sec;
		*time_ns = txtimestamp.tv_nsec;
	}

	for (int i = 0; i < ETH_PTP_TIMESTAMP_BUF_SIZE; i++){
		if (receive){
			temp = RxEthPacketRingBuf[i];
			if(p == temp.buf){
				/* Timestamp mit dem aus Ringbuffer (scheint ungenauer zu sein) überschreiben & übergeben */
				*time_s = temp.timestamp_1.tv_sec;
				*time_ns = temp.timestamp_1.tv_nsec;
				return;
			}
		}
	}
}

/**
  * @brief  Updated the PTP block for fine correction with the Time Stamp Addend register value.
  * @param  None
  * @retval None
  */
void ETH_EnablePTPTimeStampAddend(void)
{
  /* Enable the PTP block update with the Time Stamp Addend register value */
  ETH->PTPTSCR |= ETH_PTPTSCR_TSARU;
}

/**
  * @brief  Sets the Time Stamp Addend value.
  * @param  Value: specifies the PTP Time Stamp Addend Register value.
  * @retval None
  */
void ETH_SetPTPTimeStampAddend(uint32_t Value)
{
  /* Set the PTP Time Stamp Addend Register */
  ETH->PTPTSAR = Value;
}

void ETH_SetPTPTimeStampUpdate(uint32_t Sign, uint32_t SecondValue, uint32_t SubSecondValue)
{
  /* Check the parameters */
  assert_param(IS_ETH_PTP_TIME_SIGN(Sign));
  assert_param(IS_ETH_PTP_TIME_STAMP_UPDATE_SUBSECOND(SubSecondValue));
  /* Set the PTP Time Update High Register */
  ETH->PTPTSHUR = SecondValue;

  /* Set the PTP Time Update Low Register with sign */
  ETH->PTPTSLUR = Sign | SubSecondValue;
}

/*******************************************************************************
* Function Name  : ETH_PTPTimeStampAdjFreq
* Description    : Updates time stamp addend register
* Input          : Correction value in thousandth of ppm (Adj*10^9)
* Output         : None
* Return         : None
*******************************************************************************/
void ETH_PTPTime_AdjFreq(int32_t Adj)
{
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
	if( Adj > 5120000) Adj = 5120000;
	if( Adj < -5120000) Adj = -5120000;

	addend = ((((275LL * Adj)>>8) * (ADJ_FREQ_BASE_ADDEND>>24))>>6) + ADJ_FREQ_BASE_ADDEND;

	/* Reprogram the Time stamp addend register with new Rate value and set ETH_TPTSCR */
	ETH_SetPTPTimeStampAddend((uint32_t)addend);
	ETH_EnablePTPTimeStampAddend();
}

/*******************************************************************************
* Function Name  : ETH_PTPTimeStampUpdateOffset
* Description    : Updates time base offset
* Input          : Time offset with sign
* Output         : None
* Return         : None
*******************************************************************************/
void ETH_PTPTime_UpdateOffset(struct ptptime_t * timeoffset)
{
	uint32_t Sign;
	uint32_t SecondValue;
	uint32_t NanoSecondValue;
	uint32_t SubSecondValue;
	uint32_t addend;

	/* determine sign and correct Second and Nanosecond values */
	if(timeoffset->tv_sec < 0 || (timeoffset->tv_sec == 0 && timeoffset->tv_nsec < 0))
	{
		Sign = ETH_PTP_NegativeTime;
		SecondValue = -timeoffset->tv_sec;
		NanoSecondValue = -timeoffset->tv_nsec;
	}
	else
	{
		Sign = ETH_PTP_PositiveTime;
		SecondValue = timeoffset->tv_sec;
		NanoSecondValue = timeoffset->tv_nsec;
	}

	/* convert nanosecond to subseconds */
	SubSecondValue = ETH_PTPNanoSecond2SubSecond(NanoSecondValue);

	/* read old addend register value*/
	addend = ETH_GetPTPRegister(ETH_PTPTSAR_TSA); // richtiges Register?

	while(ETH_GetPTPFlagStatus(ETH_PTPTSCR_TSSTU) == SET);
	while(ETH_GetPTPFlagStatus(ETH_PTPTSCR_TSSTI) == SET);

	/* Write the offset (positive or negative) in the Time stamp update high and low registers. */
	ETH_SetPTPTimeStampUpdate(Sign, SecondValue, SubSecondValue);

	/* Set bit 3 (TSSTU) in the Time stamp control register. */
	ETH_EnablePTPTimeStampUpdate();

	/* The value in the Time stamp update registers is added to or subtracted from the system */
	/* time when the TSSTU bit is cleared. */
	while(ETH_GetPTPFlagStatus(ETH_PTPTSCR_TSSTU) == SET);

	/* Write back old addend register value. */
	ETH_SetPTPTimeStampAddend(addend);
	ETH_EnablePTPTimeStampAddend();
}



uint32_t setTimeArraySecond[100];
uint32_t setTimeArraySubSecond[100];
//uint32_t setTimeArrayIndex = 0;

/*******************************************************************************
* Function Name  : ETH_PTPTimeStampSetTime
* Description    : Initialize time base
* Input          : Time with sign
* Output         : None
* Return         : None
*******************************************************************************/
void ETH_PTPTime_SetTime(struct ptptime_t * timestamp)
{
	uint32_t Sign;
	uint32_t SecondValue;
	uint32_t NanoSecondValue;
	uint32_t SubSecondValue;

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

	/* convert nanosecond to subseconds */
	SubSecondValue = ETH_PTPNanoSecond2SubSecond(NanoSecondValue);

	setTimeArraySecond[updateArrayIndex] = SecondValue;
	setTimeArraySubSecond[updateArrayIndex] = SubSecondValue;
	//setTimeArrayIndex = (setTimeArrayIndex + 1) % 100;

	/* Write the offset (positive or negative) in the Time stamp update high and low registers. */
	ETH_SetPTPTimeStampUpdate(Sign, SecondValue, SubSecondValue);
	/* Set Time stamp control register bit 2 (Time stamp init). */
	ETH_InitializePTPTimeStamp();
	/* The Time stamp counter starts operation as soon as it is initialized
	 * with the value written in the Time stamp update register. */
	while(ETH_GetPTPFlagStatus(ETH_PTPTSCR_TSSTI) == SET);
}


FlagStatus ETH_GetPTPFlagStatus(uint32_t ETH_PTP_FLAG)
{
  return (((ETH->PTPTSCR & ETH_PTP_FLAG) != (uint32_t)RESET)) ? SET : RESET;
}


/* Private functions ---------------------------------------------------------*/
/*******************************************************************************
                       Ethernet MSP Routines
*******************************************************************************/
/**
  * @brief  Initializes the ETH MSP.
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_MspInit(ETH_HandleTypeDef *heth)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  
  /* Enable GPIOs clocks */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOG_CLK_ENABLE();

/* Ethernet pins configuration ************************************************/
  /*
        RMII_REF_CLK ----------------------> PA1
        RMII_MDIO -------------------------> PA2
        RMII_MDC --------------------------> PC1
        RMII_MII_CRS_DV -------------------> PA7
        RMII_MII_RXD0 ---------------------> PC4
        RMII_MII_RXD1 ---------------------> PC5
        RMII_MII_RXER ---------------------> PG2
        RMII_MII_TX_EN --------------------> PG11
        RMII_MII_TXD0 ---------------------> PG13
        RMII_MII_TXD1 ---------------------> PG14
  */

  /* Configure PA1, PA2 and PA7 */
  GPIO_InitStructure.Speed = GPIO_SPEED_HIGH;
  GPIO_InitStructure.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStructure.Pull = GPIO_NOPULL; 
  GPIO_InitStructure.Alternate = GPIO_AF11_ETH;
  GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_7;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStructure);
  
  /* Configure PC1, PC4 and PC5 */
  GPIO_InitStructure.Pin = GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStructure);

  /* Configure PG2, PG11, PG13 and PG14 */
  GPIO_InitStructure.Pin =  GPIO_PIN_2 | GPIO_PIN_11 | GPIO_PIN_13 | GPIO_PIN_14;
  HAL_GPIO_Init(GPIOG, &GPIO_InitStructure);
  
  /* Enable the Ethernet global Interrupt */
  HAL_NVIC_SetPriority(ETH_IRQn, 0x7, 0);
  HAL_NVIC_EnableIRQ(ETH_IRQn);
  
  /* Enable ETHERNET clock  */
  __HAL_RCC_ETH_CLK_ENABLE();
}

/**
  * @brief  Ethernet Rx Transfer completed callback
  * @param  heth: ETH handle
  * @retval None
  */
void HAL_ETH_RxCpltCallback(ETH_HandleTypeDef *heth)
{
	OSSemPost(newEthPacketSem);
}


void HAL_ETH_TxCpltCallback(ETH_HandleTypeDef *heth)
{
//	__DSB();
//
//	int i = 0;
//	struct ptptime_t timestamp;
//	ETH_PTPTime_GetTime(&timestamp);
//
//	for(i = 0; i < ETH_PTP_TIMESTAMP_BUF_SIZE; i++){
//		if(TxEthPacketRingBuf[i].dmaDescr == (void*) PTPTxDesc){
//			/* 'Abspeichern' des Timestamps, welcher in den Registern des globalen Ethhandle liegt:
//			 * ähnelte dem ok_timestamp (funktionierender Timestamp) NICHT */
//			TxEthPacketRingBuf[i].timestamp_1.tv_sec = PTPTxDesc->TimeStampHigh;
//			TxEthPacketRingBuf[i].timestamp_1.tv_nsec = ETH_PTPSubSecond2NanoSecond(PTPTxDesc->TimeStampLow);
//
//
//		}
//	}


//	TxRingBufIndex = (TxRingBufIndex + 1) % ETH_PTP_TIMESTAMP_BUF_SIZE;
}

/*******************************************************************************
                       LL Driver Interface ( LwIP stack --> ETH) 
*******************************************************************************/
/**
  * @brief In this function, the hardware should be initialized.
  * Called from ethernetif_init().
  *
  * @param netif the already initialized lwip network interface structure
  *        for this ethernetif
  */
static void low_level_init(struct netif *netif)
{
  uint8_t macaddress[6]= { MAC_ADDR0, MAC_ADDR1, MAC_ADDR2, MAC_ADDR3, MAC_ADDR4, MAC_ADDR5 };
  uint8_t err;

  EthHandle.Instance = ETH;  
  EthHandle.Init.MACAddr = macaddress;
  EthHandle.Init.AutoNegotiation = ETH_AUTONEGOTIATION_ENABLE;
  EthHandle.Init.Speed = ETH_SPEED_100M;
  EthHandle.Init.DuplexMode = ETH_MODE_FULLDUPLEX;
  EthHandle.Init.MediaInterface = ETH_MEDIA_INTERFACE_RMII;
  EthHandle.Init.RxMode = ETH_RXINTERRUPT_MODE;
  EthHandle.Init.ChecksumMode = ETH_CHECKSUM_BY_HARDWARE;
  EthHandle.Init.PhyAddress = LAN8742A_PHY_ADDRESS;
  
  /* configure ethernet peripheral (GPIOs, clocks, MAC, DMA) */
  if (HAL_ETH_Init(&EthHandle) == HAL_OK)
  {
    /* Set netif link flag */
    netif->flags |= NETIF_FLAG_LINK_UP;
  }
  
  /* Initialize Tx Descriptors list: Chain Mode */
  HAL_ETH_DMATxDescListInit(&EthHandle, DMATxDscrTab, &Tx_Buff[0][0], ETH_TXBUFNB);
     
  /* Initialize Rx Descriptors list: Chain Mode  */
  HAL_ETH_DMARxDescListInit(&EthHandle, DMARxDscrTab, &Rx_Buff[0][0], ETH_RXBUFNB);
  
  /* set netif MAC hardware address length */
  netif->hwaddr_len = ETHARP_HWADDR_LEN;

  /* set netif MAC hardware address */
  netif->hwaddr[0] =  MAC_ADDR0;
  netif->hwaddr[1] =  MAC_ADDR1;
  netif->hwaddr[2] =  MAC_ADDR2;
  netif->hwaddr[3] =  MAC_ADDR3;
  netif->hwaddr[4] =  MAC_ADDR4;
  netif->hwaddr[5] =  MAC_ADDR5;

  /* set netif maximum transfer unit */
  netif->mtu = 1500;

  /* Accept broadcast address and ARP traffic */
  netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;

  /*igmp*/
  netif->flags |= NETIF_FLAG_IGMP;

  /* create a binary semaphore used for informing ethernetif of frame reception */
  newEthPacketSem = OSSemCreate(1);

  OSTaskCreateExt( ETHInputTask,                              /* Create the start task                                */
                   (void*)netif,
                  &eth_input_task_stk[ETH_INPUT_TASK_STACK_SIZE - 1],
                   ETH_INPUT_TASK_PRIO,
				   ETH_INPUT_TASK_PRIO,
                  &eth_input_task_stk[0],
				  ETH_INPUT_TASK_STACK_SIZE,
                   0,
                  (OS_TASK_OPT_STK_CHK | OS_TASK_OPT_STK_CLR));

#if (OS_TASK_NAME_EN > 0)
    OSTaskNameSet(ETH_INPUT_TASK_PRIO,(INT8U *)ETH_INPUT_TASK_NAME,&err);
#endif

  /* Enable PTP Timestamping */
  ETH_PTPStart();

  /* Enable MAC and DMA transmission and reception */
  HAL_ETH_Start(&EthHandle);
}



/*
 * @brief: This function reads the PHY Register and notify the Ethernet Interface when a change of link state occurs.
 * To get notified on changes use ethernet_changed_callback
 *
 * @param netif the already initialized lwip network interface structure
 *       for this ethernetif
 *
 * @param  heth: ETH handle
 */
static uint8_t low_level_check_link_state(ETH_HandleTypeDef *heth, struct netif *netif) {

	uint8_t err = HAL_ERROR;
	uint32_t phyreg = 0;

	/*	Read PHY Regsiter to get Link State	*/
	 err = HAL_ETH_ReadPHYRegister(heth, PHY_BSR, &phyreg);
	 if(err != HAL_OK) {
		 return HAL_ERROR;
	 }

	 if (((phyreg & PHY_LINKED_STATUS) != PHY_LINKED_STATUS)) { /*	Link is down */
		if (netif->flags & NETIF_FLAG_LINK_UP) {	/*	Link was up, it changed, set link down	*/
			netif_set_link_down(netif);
		}
	 }
	 else {		/*	Link is up	*/
		 if (!(netif->flags & NETIF_FLAG_LINK_UP)) {	/*	Link was down, it changed, set link up	*/
			 netif_set_link_up(netif);
		 }
	 }

}

/**
  * @brief This function should do the actual transmission of the packet. The packet is
  * contained in the pbuf that is passed to the function. This pbuf
  * might be chained.
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
  * @return ERR_OK if the packet could be sent
  *         an err_t value if the packet couldn't be sent
  *
  * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
  *       strange results. You might consider waiting for space in the DMA queue
  *       to become available since the stack doesn't retry to send a packet
  *       dropped because of memory failure (except for the TCP timers).
  */
err_t low_level_output(struct netif *netif, struct pbuf *p)
{
  err_t errval;
  struct pbuf *q;
  uint8_t *buffer = (uint8_t *)(EthHandle.TxDesc->Buffer1Addr);
  __IO ETH_DMADescTypeDef *DmaTxDesc;
  uint32_t framelength = 0;
  uint32_t bufferoffset = 0;
  uint32_t byteslefttocopy = 0;
  uint32_t payloadoffset = 0;

  uint64_t sec = 0;
  uint64_t us = 0;

  DmaTxDesc = EthHandle.TxDesc;
  PTPTxDesc = DmaTxDesc;



  bufferoffset = 0;
  
  /* copy frame from pbufs to driver buffers */
  for(q = p; q != NULL; q = q->next)
  {
    /* Is this buffer available? If not, goto error */
    if((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
    {
      errval = ERR_USE;
      goto error;
    }
    
    /* Get bytes in current lwIP buffer */
    byteslefttocopy = q->len;
    payloadoffset = 0;
    
    /* Check if the length of data to copy is bigger than Tx buffer size*/
    while( (byteslefttocopy + bufferoffset) > ETH_TX_BUF_SIZE )
    {
      /* Copy data to Tx buffer*/
      memcpy( (uint8_t*)((uint8_t*)buffer + bufferoffset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), (ETH_TX_BUF_SIZE - bufferoffset) );
      
      DmaTxDesc->Status |= ETH_DMATXDESC_TTSE;	// ETH_DMATXDESC_TTSE Timestamping enable

      /* Point to next descriptor */
      DmaTxDesc = (ETH_DMADescTypeDef *)(DmaTxDesc->Buffer2NextDescAddr);
      

      /* Check if the buffer is available */
      if((DmaTxDesc->Status & ETH_DMATXDESC_OWN) != (uint32_t)RESET)
      {
        errval = ERR_USE;
        goto error;
      }
      
      buffer = (uint8_t *)(DmaTxDesc->Buffer1Addr);
      
      byteslefttocopy = byteslefttocopy - (ETH_TX_BUF_SIZE - bufferoffset);
      payloadoffset = payloadoffset + (ETH_TX_BUF_SIZE - bufferoffset);
      framelength = framelength + (ETH_TX_BUF_SIZE - bufferoffset);
      bufferoffset = 0;
    }
    
    /* Copy the remaining bytes */
    memcpy( (uint8_t*)((uint8_t*)buffer + bufferoffset), (uint8_t*)((uint8_t*)q->payload + payloadoffset), byteslefttocopy );
    bufferoffset = bufferoffset + byteslefttocopy;
    framelength = framelength + byteslefttocopy;
  }


	ETH_PTPTime_GetTime(&txtimestamp);
	/* save buffer p as Ringbuffer Entry; Timestamp to follow in TxCallBack-Function */
//	TxEthPacketRingBuf[TxRingBufIndex].buf = p;
//	TxEthPacketRingBuf[TxRingBufIndex].dmaDescr = (void*) PTPTxDesc;
//	TxEthPacketRingBuf[TxRingBufIndex].timestamp_2.tv_nsec = timestamp.tv_nsec;
//	TxEthPacketRingBuf[TxRingBufIndex].timestamp_2.tv_sec = timestamp.tv_sec;
//	TxRingBufIndex = (TxRingBufIndex + 1) % ETH_PTP_TIMESTAMP_BUF_SIZE;
//
//	if(TxRingBufIndex == 0)
//		asm volatile ("nop");

  /* Clean and Invalidate data cache */
  SCB_CleanInvalidateDCache();  
  /* Prepare transmit descriptors to give to DMA */ 
  HAL_ETH_TransmitFrame(&EthHandle, framelength);
  

  errval = ERR_OK;
  
error:
  
  /* When Transmit Underflow flag is set, clear it and issue a Transmit Poll Demand to resume transmission */
  if ((EthHandle.Instance->DMASR & ETH_DMASR_TUS) != (uint32_t)RESET)
  {
    /* Clear TUS ETHERNET DMA flag */
    EthHandle.Instance->DMASR = ETH_DMASR_TUS;
    
    /* Resume DMA transmission*/
    EthHandle.Instance->DMATPDR = 0;
  }
  return errval;
}

/**
  * @brief Should allocate a pbuf and transfer the bytes of the incoming
  * packet from the interface into the pbuf.
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @return a pbuf filled with the received packet (including MAC header)
  *         NULL on memory error
  */
static struct pbuf * low_level_input(struct netif *netif)
{
  struct pbuf *p = NULL, *q = NULL;
  uint16_t len = 0;
  uint8_t *buffer;
  __IO ETH_DMADescTypeDef *dmarxdesc;
  uint32_t bufferoffset = 0;
  uint32_t payloadoffset = 0;
  uint32_t byteslefttocopy = 0;
  uint32_t i=0;

  /* get received frame */
  if(HAL_ETH_GetReceivedFrame_IT(&EthHandle) != HAL_OK)
    return NULL;
  
  /* Obtain the size of the packet and put it into the "len" variable. */
  len = EthHandle.RxFrameInfos.length;
  buffer = (uint8_t *)EthHandle.RxFrameInfos.buffer;
  
  if (len > 0)
  {
    /* We allocate a pbuf chain of pbufs from the Lwip buffer pool */
    p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
  }
  
  /* Clean and Invalidate data cache */
  SCB_CleanInvalidateDCache();
  
  if (p != NULL)
  {
    dmarxdesc = EthHandle.RxFrameInfos.FSRxDesc;
    bufferoffset = 0;
    
	ptpRxTimeHigh = dmarxdesc->TimeStampHigh;
	ptpRxTimeLow = dmarxdesc->TimeStampLow;

	RxEthPacketRingBuf[RxRingBufIndex].timestamp_1.tv_sec = ptpRxTimeHigh;
	RxEthPacketRingBuf[RxRingBufIndex].timestamp_1.tv_nsec = ETH_PTPSubSecond2NanoSecond(ptpRxTimeLow);
	RxEthPacketRingBuf[RxRingBufIndex].buf = p;
	RxRingBufIndex = (RxRingBufIndex + 1) % ETH_PTP_TIMESTAMP_BUF_SIZE;

    for(q = p; q != NULL; q = q->next)
    {
      byteslefttocopy = q->len;
      payloadoffset = 0;
      
      /* Check if the length of bytes to copy in current pbuf is bigger than Rx buffer size */
      while( (byteslefttocopy + bufferoffset) > ETH_RX_BUF_SIZE )
      {
        /* Copy data to pbuf */
        memcpy( (uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)buffer + bufferoffset), (ETH_RX_BUF_SIZE - bufferoffset));
        
        /* Point to next descriptor */
        dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
        buffer = (uint8_t *)(dmarxdesc->Buffer1Addr);
        

        byteslefttocopy = byteslefttocopy - (ETH_RX_BUF_SIZE - bufferoffset);
        payloadoffset = payloadoffset + (ETH_RX_BUF_SIZE - bufferoffset);
        bufferoffset = 0;
      }
      
      /* Copy remaining data in pbuf */
      memcpy( (uint8_t*)((uint8_t*)q->payload + payloadoffset), (uint8_t*)((uint8_t*)buffer + bufferoffset), byteslefttocopy);
      bufferoffset = bufferoffset + byteslefttocopy;
    }
  }

	if(RxRingBufIndex == 0)
		asm volatile("nop");

  /* Release descriptors to DMA */
  /* Point to first descriptor */
  dmarxdesc = EthHandle.RxFrameInfos.FSRxDesc;
  /* Set Own bit in Rx descriptors: gives the buffers back to DMA */
  for (i=0; i< EthHandle.RxFrameInfos.SegCount; i++)
  {  
    dmarxdesc->Status |= ETH_DMARXDESC_OWN;
    dmarxdesc = (ETH_DMADescTypeDef *)(dmarxdesc->Buffer2NextDescAddr);
  }
    
  /* Clear Segment_Count */
  EthHandle.RxFrameInfos.SegCount =0;
  
  /* When Rx Buffer unavailable flag is set: clear it and resume reception */
  if ((EthHandle.Instance->DMASR & ETH_DMASR_RBUS) != (uint32_t)RESET)  
  {
    /* Clear RBUS ETHERNET DMA flag */
    EthHandle.Instance->DMASR = ETH_DMASR_RBUS;
    /* Resume DMA reception */
    EthHandle.Instance->DMARPDR = 0;
  }
  return p;
}

/**
  * @brief This function is the ethernetif_input task, it is processed when a packet 
  * is ready to be read from the interface. It uses the function low_level_input() 
  * that should handle the actual reception of bytes from the network
  * interface. Then the type of the received packet is determined and
  * the appropriate input function is called.
  *
  * @param netif the lwip network interface structure for this ethernetif
  */
static void ETHInputTask( void * argument )
{
  struct pbuf *p;
  struct netif *netif = (struct netif *) argument;
  
  INT8U pendErr = OS_ERR_NONE;
  for( ;; )
  {


    OSSemPend(newEthPacketSem, 1000, &pendErr);
    if(pendErr == OS_ERR_TIMEOUT) {
    	low_level_check_link_state(&EthHandle, netif);
    }
    else if(pendErr == OS_ERR_NONE)
    {
      do
      {
        p = low_level_input( netif );
        if (p != NULL)
        {
          if (netif->input( p, netif) != ERR_OK )
          {
            pbuf_free(p);
          }
        }
      }while(p!=NULL);
    }
  }
}

/**
  * @brief Should be called at the beginning of the program to set up the
  * network interface. It calls the function low_level_init() to do the
  * actual setup of the hardware.
  *
  * This function should be passed as a parameter to netif_add().
  *
  * @param netif the lwip network interface structure for this ethernetif
  * @return ERR_OK if the loopif is initialized
  *         ERR_MEM if private data couldn't be allocated
  *         any other err_t on error
  */
err_t ethernetif_init(struct netif *netif)
{
  LWIP_ASSERT("netif != NULL", (netif != NULL));

#if LWIP_NETIF_HOSTNAME
  /* Initialize interface hostname */
  netif->hostname = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */

  netif->name[0] = IFNAME0;
  netif->name[1] = IFNAME1;

  /* We directly use etharp_output() here to save a function call.
   * You can instead declare your own function an call etharp_output()
   * from it if you have to do some checks before sending (e.g. if link
   * is available...) */
  netif->output = etharp_output;
  netif->linkoutput = low_level_output;

  /* initialize the hardware */
  low_level_init(netif);

  return ERR_OK;
}

static void ETH_PTPStart (){

	/* Mask the Time stamp trigger interrupt by setting bit 9 in the MACIMR register. */
	__HAL_ETH_MAC_ENABLE_IT(&EthHandle, ETH_MAC_IT_TST);		// or disable?

	/* Program Time stamp register bit 0 to enable time stamping. */
	//ETH_PTPTimeStampCmd(ENABLE);
	//ENTSPRICHT:
		/* Enable the PTP time stamp for transmit and receive frames */
		ETH->PTPTSCR |= ETH_PTPTSCR_TSE | ETH_PTPTSSR_TSSIPV4FE | ETH_PTPTSSR_TSSIPV6FE | ETH_PTPTSSR_TSSARFE;  // ETH_PTPTSSR_TSSARFE TimeStamp Snapshot for all received frames

	/* Program the Subsecond increment register based on the PTP clock frequency. */
	//ETH_SetPTPSubSecondIncrement(ADJ_FREQ_BASE_INCREMENT); /* to achieve 20 ns accuracy, the value is ~ 43 */
	// ENTPSRICHT:
		/* Set the PTP Sub-Second Increment Register */
		ETH->PTPSSIR = ADJ_FREQ_BASE_INCREMENT;

	/* If you are using the Fine correction method, program the Time stamp addend register
	     * and set Time stamp control register bit 5 (addend register update). */
	//ETH_SetPTPTimeStampAddend(ADJ_FREQ_BASE_ADDEND);
	//ETH_EnablePTPTimeStampAddend();
	//ENTSPRICHT:
//		/* Set the PTP Time Stamp Addend Register */
		ETH->PTPTSAR = ADJ_FREQ_BASE_ADDEND;
//		/* Enable the PTP block update with the Time Stamp Addend register value */
		ETH->PTPTSCR |= ETH_PTPTSCR_TSARU;
//		/* Poll the Time stamp control register until bit 5 is cleared. */
		while(ETH_GetPTPFlagStatus(ETH_PTPTSCR_TSARU) == SET);

	/* To select the Fine correction method (if required),
	 * program Time stamp control register  bit 1. */
	//ETH_PTPUpdateMethodConfig(UpdateMethod);
//	//ENTSPRICHT:
//		/* Enable the PTP Fine Update method // CoarseUpdateMeth not implemented*/
		ETH->PTPTSCR |= ETH_PTPTSCR_TSFCU;

	/* Program the Time stamp high update and Time stamp low update registers
	 * with the appropriate time value. */
	//ETH_SetPTPTimeStampUpdate(ETH_PTP_PositiveTime, 0, 0);
	//ENTSPRICHT:
		/* Set the PTP Time Update High Register */
		ETH->PTPTSHUR = 0;

		/* Set the PTP Time Update Low Register with sign */
		ETH->PTPTSLUR = ETH_PTP_PositiveTime | 0;

	/* Set Time stamp control register bit 2 (Time stamp init). */
	//ETH_InitializePTPTimeStamp();
	//ENTSPRICHT:
		/* Initialize the PTP Time Stamp */
		ETH->PTPTSCR |= ETH_PTPTSCR_TSSTI;

	 /* The enhanced descriptor format is enabled and the descriptor size is
	  * increased to 32 bytes (8 DWORDS). This is required when time stamping
	  * is activated above. */
	//ETH_EnhancedDescriptorCmd(ENABLE);
	//ENTSPRICHT:
		/* Enable enhanced descriptor structure */
		ETH->DMABMR |= ETH_DMABMR_EDE;

	  /* The Time stamp counter starts operation as soon as it is initialized
	   * with the value written in the Time stamp update register. */

}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
