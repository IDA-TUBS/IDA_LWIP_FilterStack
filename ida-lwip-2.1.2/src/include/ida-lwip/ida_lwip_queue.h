

#ifndef _IDA_LWIP_IDA_LWIP_QUEUE_H_
#define _IDA_LWIP_IDA_LWIP_QUEUE_H_


#include "lwip/sys.h"

/**
 * Size of each message queue in words
 */
#ifndef IDA_LWIP_QUEUE_SIZE
#define IDA_LWIP_QUEUE_SIZE	16
#endif

/**
 * Number of message queues available
 */
#ifndef IDA_LWIP_QUEUE_COUNT
#define IDA_LWIP_QUEUE_COUNT	32
#endif

typedef struct {
  u16_t count;		/** Number of entries in queue */
  u16_t trigger;	/** trigger level for monitor */
  u16_t head;		/** head of queue */
  u16_t tail;		/** tail of queue */
  u16_t max;		/** maximum entries used */
  u16_t size;		/** size of queue */
  u32_t data[IDA_LWIP_QUEUE_SIZE];
} IDA_LWIP_QUEUE;



void ida_lwip_queue_init(void);
IDA_LWIP_QUEUE * ida_lwip_queue_alloc(u16_t size);
int ida_lwip_queue_put(IDA_LWIP_QUEUE* queue, void* data);
void * ida_lwip_queue_get(IDA_LWIP_QUEUE* queue);
void ida_lwip_queue_set_trigger(IDA_LWIP_QUEUE* queue, u16_t trigger);
void ida_lwip_queue_free(IDA_LWIP_QUEUE* queue);

#endif //_IDA_LWIP_IDA_LWIP_QUEUE_H_
