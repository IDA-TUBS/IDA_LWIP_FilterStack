

#ifndef __NETIF_IDA_LWIP_VIRTETH_MASTER_H__
#define __NETIF_IDA_LWIP_VIRTETH_MASTER_H__

#include "lwip/netif.h"
#include "netif/etharp.h"
#include "lwip/sys.h"
#include "ida_lwip_virtEth.h"

#define	TX_BUFFER_ENTRY_SIZE	1502
#define TX_BUFFER_ENTRY_COUNT	20
#define TX_BUFFER_BASE			0xFFFD000

#if TX_BUFFER_ENTRY_COUNT > IDA_LWIP_MEM_QUEUE_SIZE
#error "the queue must be able to store all entries at once"
#endif

void ida_lwip_virtEth_master_init(void);
void ida_lwip_virtEth_receiveFromClassic(sys_sem_t txCompleteSem);
void ida_lwip_virtEth_sendToClassic(struct pbuf* p);

#endif /* __NETIF_IDA_LWIP_VIRTETH_H__ */
