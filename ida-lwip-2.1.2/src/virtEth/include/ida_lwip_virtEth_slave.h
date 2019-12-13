

#ifndef __NETIF_IDA_LWIP_VIRTETH_SLAVE_H__
#define __NETIF_IDA_LWIP_VIRTETH_SLAVE_H__

#include "lwip/netif.h"
#include "netif/etharp.h"
#include "lwip/sys.h"
#include "lwip/pbuf.h"

#include "ida_lwip_virtEth.h"

#define	RX_BUFFER_ENTRY_SIZE	1502
#define RX_BUFFER_ENTRY_COUNT	20
#define RX_BUFFER_BASE			0xFFFE000

#if RX_BUFFER_ENTRY_COUNT > IDA_LWIP_MEM_QUEUE_SIZE
#error "the queue must be able to store all entries at once"
#endif

typedef struct RX_PBUF
{
   struct pbuf_custom p;
   void *data;
} RX_PBUF_T;


void ida_lwip_virtEth_rxTask(void *p_args);
err_t ida_lwip_virtEth_init(struct netif *netif);

#endif /* __NETIF_IDA_LWIP_VIRTETH_H__ */
