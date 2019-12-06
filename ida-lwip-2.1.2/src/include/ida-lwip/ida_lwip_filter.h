/*
 * ida_lwip_filter.h
 *
 *  Created on: Jun 11, 2019
 *      Author: noras
 */

#ifndef _IDA_LWIP_IDA_LWIP_FILTER_H_
#define _IDA_LWIP_IDA_LWIP_FILTER_H_

#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/ip.h"

#define IDA_FILTER_MBOX_SIZE 8
#define MBOX_SEM_TIMEOUT 0 //wait forever

#ifndef IDA_LWIP_TX_FILTER_STACK_SIZE
#define IDA_LWIP_TX_FILTER_STACK_SIZE 	256
#endif

#ifndef IDA_LWIP_RX_FILTER_STACK_SIZE
#define IDA_LWIP_RX_FILTER_STACK_SIZE 	256
#endif

#ifndef IDA_LWIP_CLASSIC_ADAPTER_STACK_SIZE
#define IDA_LWIP_CLASSIC_ADAPTER_STACK_SIZE 	256
#endif


typedef struct IDA_LWIP_FILTER_PBUF
{
   struct pbuf_custom p;
   sys_sem_t tx_complete_sem;
} IDA_LWIP_FILTER_PBUF;

typedef struct{
	enum {UDP, RAW} type;
	void *data;
	size_t size;
	int socket;
	void *to;
	int err;
	sys_sem_t txCompleteSem;
}IDA_LWIP_TX_REQ;

err_t ida_filter_enqueue_pkt(void *data, u8_t prio, u8_t direction);
ssize_t ida_lwip_send_raw(void *data, size_t size, sys_sem_t *completeSem);
void ida_filter_init(struct netif *netif);


#endif /* _IDA_LWIP_IDA_LWIP_FILTER_H_ */
