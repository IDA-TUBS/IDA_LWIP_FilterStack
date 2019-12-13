

#ifndef __NETIF_IDA_LWIP_VIRTETH_H__
#define __NETIF_IDA_LWIP_VIRTETH_H__

#include "lwip/netif.h"
#include "netif/etharp.h"
#include "lwip/sys.h"

#define IDA_LWIP_MEM_FROM_CLASSIC_BASE	0xFFFC000
#define IDA_LWIP_MEM_FROM_CLASSIC_SIZE	0x1000
#define IDA_LWIP_MEM_TO_CLASSIC_BASE	0xFFFD000
#define IDA_LWIP_MEM_TO_CLASSIC_SIZE	0x1000
#define IDA_LWIP_MEM_QUEUE_SIZE			32

#define CEIL_ALIGN(x) 	x%sizeof(u32_t)==0 ? (x) : ((x/sizeof(u32_t))+sizeof(u32_t))

typedef struct{
	size_t 	size;
	void 	*data;
	void	*ref;
} IDA_LWIP_IPI_QUEUE_ENTRY;

typedef struct{
	u32_t lock;
	u8_t	head;
	u8_t	tail;
	u8_t	used;
	u8_t	maxUsed;
	IDA_LWIP_IPI_QUEUE_ENTRY	data[IDA_LWIP_MEM_QUEUE_SIZE];
} IDA_LWIP_IPI_QUEUE;

typedef struct{
	IDA_LWIP_IPI_QUEUE		freeRxBuffers;
	IDA_LWIP_IPI_QUEUE		freeTxBuffers;	/* free buffers that can be filled by classic stack */
	IDA_LWIP_IPI_QUEUE		rxBuffers;		/* RX Packets to classic stack */
	IDA_LWIP_IPI_QUEUE		txBuffers;		/* TX Packets from classic stack */
	u32_t head;					/* Next to write (RW,R-) */
	u32_t tail;					/* Next to read	 (R-,RW) */
	u32_t free_tail;			/* Next to free  (RW,--) */
	u32_t ringBufferSize;		/* Size in bytes (const) */
	u32_t ringBufferFree;		/* Free bytes 	 (RW,--) */
	u32_t *ringBufferBase;
} SHARED_MEMORY_MGMT;

typedef struct{
	void *next;
	void *data;
} IPI_BUFFER_DESCR;

void spin_lock(u32_t *l);
void spin_unlock(u32_t *l);
u8_t ida_lwip_virtEth_queuePut(IDA_LWIP_IPI_QUEUE *queue, IDA_LWIP_IPI_QUEUE_ENTRY *entry);
u8_t ida_lwip_virtEth_queueGet(IDA_LWIP_IPI_QUEUE *queue, IDA_LWIP_IPI_QUEUE_ENTRY *entry);
u8_t ida_lwip_virtEth_queueInit(IDA_LWIP_IPI_QUEUE *queue);

#endif /* __NETIF_IDA_LWIP_VIRTETH_H__ */
