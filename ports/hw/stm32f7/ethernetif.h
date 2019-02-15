#ifndef __ETHERNETIF_H__
#define __ETHERNETIF_H__

#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/netif.h"
#include "ucos_ii.h"
#include "stm32f7xx_hal_eth.h"


/* Exported types ------------------------------------------------------------*/

struct ptptime_t {
  s32_t tv_sec;
  s32_t tv_nsec;
};

typedef struct {
	s32_t diff;
	struct ptptime_t ok_timestamp;
	struct ptptime_t timestamp_2;
	struct ptptime_t timestamp_1;
	struct ptptime_t timestamp_3;
	struct ptptime_t timestamp_4;
	void*		dmaDescr;
	struct pbuf* buf;

}EthPacketTimestamp;

#ifndef ETH_PTP_TIMESTAMP_BUF_SIZE
#define ETH_PTP_TIMESTAMP_BUF_SIZE 20
#endif

/* Exported functions ------------------------------------------------------- */
err_t ethernetif_init(struct netif *netif);
err_t low_level_output_old(struct netif *netif, struct pbuf *p, ETH_DMADescTypeDef *DmaTxDesc);
FlagStatus ETH_GetPTPFlagStatus(uint32_t ETH_PTP_FLAG);
void ETH_PTP_GetTimestamp(struct pbuf* p, int32_t *time_s, int32_t *time_ns, BOOLEAN receive);

#define ADJ_FREQ_BASE_ADDEND      0x3B425ED1
#define ADJ_FREQ_BASE_INCREMENT   86


void ETH_InitializePTPTimeStamp(void);
void ETH_PTPTime_GetTime(struct ptptime_t * timestamp);
void ETH_EnablePTPTimeStampAddend(void);
void ETH_SetPTPTimeStampAddend(uint32_t Value);
void ETH_SetPTPTimeStampUpdate(uint32_t Sign, uint32_t SecondValue, uint32_t SubSecondValue);
void ETH_PTPTime_AdjFreq(int32_t Adj);
void ETH_PTPTime_UpdateOffset(struct ptptime_t * timeoffset);
void ETH_PTPTime_SetTime(struct ptptime_t * timestamp);

uint32_t updateArrayIndex;



/* Examples of subsecond increment and addend values using SysClk = 144 MHz

 Addend * Increment = 2^63 / SysClk

 ptp_tick = Increment * 10^9 / 2^31

 +-----------+-----------+------------+
 | ptp tick  | Increment | Addend     |
 +-----------+-----------+------------+
 |  119 ns   |   255     | 0x0EF8B863 |
 |  100 ns   |   215     | 0x11C1C8D5 |
 |   50 ns   |   107     | 0x23AE0D90 |
 |   20 ns   |    43     | 0x58C8EC2B |
 |   14 ns   |    30     | 0x7F421F4F |
 +-----------+-----------+------------+
*/

/* Examples of subsecond increment and addend values using SysClk = 168 MHz

 Addend * Increment = 2^63 / SysClk

 ptp_tick = Increment * 10^9 / 2^31

 +-----------+-----------+------------+
 | ptp tick  | Increment | Addend     |
 +-----------+-----------+------------+
 |  119 ns   |   255     | 0x0CD53055 |
 |  100 ns   |   215     | 0x0F386300 |
 |   50 ns   |   107     | 0x1E953032 |
 |   20 ns   |    43     | 0x4C19EF00 |
 |   14 ns   |    30     | 0x6D141AD6 |
 +-----------+-----------+------------+
*/

/* Examples of subsecond increment and addend values using SysClk = 216MHz
 *
 * 216/50 = 4.32
 * 2^32 / 4.32 = 0x3B425ED1

 Addend * Increment = 2^63 / SysClk = 85401592934
 Increment = 85401592934 / Addend = 86

 ptp_tick = Increment * 10^9 / 2^31

 +-----------+-----------+------------+
 | ptp tick  | Increment | Addend     |
 +-----------+-----------+------------+
 |   20 ns   |    43     | 0x3FECD300 |

*/



#endif
