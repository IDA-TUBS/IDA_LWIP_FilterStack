/*
 * ida_lwip_monitor.h
 *
 *  Created on: 12.06.2019
 *      Author: kaige
 */

#ifndef SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_MONITOR_H_
#define SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_MONITOR_H_

#include "lwip/sys.h"
#include "lwip/pbuf.h"

typedef struct PBUF_MONITOR
{
   u16_t counter;
   u16_t trigger;
} PBUF_MONITOR_T;

typedef struct MONITORED_PBUF
{
   struct pbuf_custom p;
   PBUF_MONITOR_T* monitor_ref;
} MONITORED_PBUF_T;

struct pbuf * ida_monitored_pbuf_alloc(u16_t length);
void ida_monitor_set_trigger(PBUF_MONITOR_T *monitor, u16_t trigger);
int ida_monitor_check(struct pbuf *p, PBUF_MONITOR_T *monitor);
PBUF_MONITOR_T * ida_monitor_alloc(u16_t trigger);
void ida_monitor_free(PBUF_MONITOR_T *monitor);
void ida_monitor_init(void);


#endif /* SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_MONITOR_H_ */
