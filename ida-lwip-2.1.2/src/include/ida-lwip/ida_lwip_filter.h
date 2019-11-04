/*
 * ida_lwip_filter.h
 *
 *  Created on: Jun 11, 2019
 *      Author: noras
 */

#ifndef SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_FILTER_H_
#define SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_FILTER_H_

#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/ip.h"

#define IDA_FILTER_MBOX_SIZE 8
#define MBOX_SEM_TIMEOUT 0 //wait forever

typedef struct{
	sys_mbox_t mbox;
	u8_t count;
}IDA_LWIP_FILTER_MBOX;

typedef struct{
	IDA_LWIP_FILTER_MBOX filter_mbox[8];
	u8_t prio_field;
	sys_sem_t act_sem;
}IDA_LWIP_FILTER_QUEUE;

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
void ida_filter_init(struct netif *netif);


#endif /* SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_FILTER_H_ */
