

#include <stdio.h>
#include <string.h>

#include "lwipopts.h"
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/mem.h"
#include "lwip/pbuf.h"
#include "lwip/sys.h"
#include "lwip/stats.h"

#include "ida_lwip_virtEth.h"

void spin_lock(u32_t *l) {
  while (__atomic_test_and_set(l, __ATOMIC_ACQUIRE));;
}

void spin_unlock(u32_t *l){
	__atomic_clear(l, __ATOMIC_RELEASE);
}

u8_t ida_lwip_virtEth_queuePut(IDA_LWIP_IPI_QUEUE *queue, IDA_LWIP_IPI_QUEUE_ENTRY *entry){
	spin_lock(&queue->lock);
	if(queue->used == IDA_LWIP_MEM_QUEUE_SIZE){
		spin_unlock(&queue->lock);
		return 0;
	}
	memcpy((void*)&queue->data[queue->head],(void*)entry,sizeof(IDA_LWIP_IPI_QUEUE_ENTRY));
	queue->used++;
	if(queue->used > queue->maxUsed)
		queue->maxUsed = queue->used;
	queue->head = (queue->head + 1) % IDA_LWIP_MEM_QUEUE_SIZE;
	spin_unlock(&queue->lock);
	return 1;
}

u8_t ida_lwip_virtEth_queueGet(IDA_LWIP_IPI_QUEUE *queue, IDA_LWIP_IPI_QUEUE_ENTRY *entry){
	spin_lock(&queue->lock);
	if(queue->used == 0){
		spin_unlock(&queue->lock);
		return 0;
	}
	memcpy((void*)entry,(void*)&queue->data[queue->tail],sizeof(IDA_LWIP_IPI_QUEUE_ENTRY));
	queue->used--;
	queue->tail = (queue->tail + 1) % IDA_LWIP_MEM_QUEUE_SIZE;
	spin_unlock(&queue->lock);
	return 1;
}

u8_t ida_lwip_virtEth_queueInit(IDA_LWIP_IPI_QUEUE *queue){
	memset((void*)queue, 0, sizeof(IDA_LWIP_IPI_QUEUE));
}


