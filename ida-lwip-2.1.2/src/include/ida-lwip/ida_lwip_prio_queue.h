/*
 * ida_lwip_prio_queue.h
 *
 *  Created on: 21.06.2019
 *      Author: kaige
 */

#ifndef SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_PRIO_QUEUE_H_
#define SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_PRIO_QUEUE_H_

#include "lwip/sys.h"

typedef struct{
	sys_mbox_t mbox[8];
	u16_t count[8];
	u8_t prio_field;
	sys_sem_t act_sem;
}IDA_LWIP_PRIO_QUEUE;

void ida_lwip_prioQueueInit(void);
IDA_LWIP_PRIO_QUEUE* ida_lwip_prioQueueCreate(int size);
void ida_lwip_prioQueueDestroy(IDA_LWIP_PRIO_QUEUE* queue);
err_t ida_lwip_prioQueuePut(IDA_LWIP_PRIO_QUEUE *queue, void *msg, u8_t prio);
void *ida_lwip_prioQueuePend(IDA_LWIP_PRIO_QUEUE *queue, u32_t timeout);

#endif /* SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_PRIO_QUEUE_H_ */
