/*
 * ida_lwip_prio_queue.c
 *
 *  Created on: 21.06.2019
 *      Author: kaige
 */

#include "lwip/sys.h"
#include "ida-lwip/ida_lwip_prio_queue.h"
#include "lwip/memp.h"
#include <string.h>

LWIP_MEMPOOL_DECLARE(PRIO_QUEUE_POOL, 20, sizeof(IDA_LWIP_PRIO_QUEUE), "Priority Queue Pool");

/*
 * funtion to initialize the memory pool for the priority queue
 *
 * */
void ida_lwip_prioQueueInit(void){
	LWIP_MEMPOOL_INIT(PRIO_QUEUE_POOL);
}

/*
 * function to free the prority queue
 *
 * @param queue: priority queue to free
 * */
void ida_lwip_prioQueueDestroy(IDA_LWIP_PRIO_QUEUE* queue){
	if(queue == (IDA_LWIP_PRIO_QUEUE*)NULL)
		return;

	for(int i = 0; i < 8; i++){
		if(&queue->mbox[i] != NULL)
			sys_mbox_free(&queue->mbox[i]);
	}

	if(&queue->act_sem != NULL)
		sys_sem_free(&queue->act_sem);

	LWIP_MEMPOOL_FREE(PRIO_QUEUE_POOL, queue);
}

/*
 * function to create the prority queue
 *
 * @param size: size of the message boxes for each priority
 *
 * @return priority queue
 * */
IDA_LWIP_PRIO_QUEUE* ida_lwip_prioQueueCreate(int size){
	IDA_LWIP_PRIO_QUEUE* queue = (IDA_LWIP_PRIO_QUEUE*)LWIP_MEMPOOL_ALLOC(PRIO_QUEUE_POOL);
	if(queue == (IDA_LWIP_PRIO_QUEUE*)NULL)
		return (IDA_LWIP_PRIO_QUEUE*)NULL;

	memset(queue,0,sizeof(IDA_LWIP_PRIO_QUEUE));

	for(int i = 0; i < 8; i++){
		queue->count[i] = 0;
		if(sys_mbox_new(&queue->mbox[i], size) != ERR_OK){
			ida_lwip_prioQueueDestroy(queue);
			return NULL;
		}
	}
	queue->prio_field = 0;
	if(sys_sem_new(&queue->act_sem,0) != ERR_OK){
		ida_lwip_prioQueueDestroy(queue);
		return NULL;
	}

	return queue;
}

/*
 * function to queue message into the message box for a certain priority
 *
 * @param queue: pointer of priority queue
 * @param msg: mesasge to queue
 * @param: prio: priority of the message
 *
 * @return err
 * */
inline err_t ida_lwip_prioQueuePut(IDA_LWIP_PRIO_QUEUE *queue, void *msg, u8_t prio){
	CPU_SR cpu_sr;

	if(prio > 7){
		return ERR_ARG;
	}
	OS_ENTER_CRITICAL();

	if (queue->count[prio] == IDA_LWIP_MBOX_SIZE){
		OS_EXIT_CRITICAL();
		return ERR_MEM;
	}

	sys_mbox_trypost(&queue->mbox[prio], msg);
	queue->count[prio]++;
	queue->prio_field |= (1 << prio);
	sys_sem_signal(&queue->act_sem);
	OS_EXIT_CRITICAL();

	return ERR_OK;
}

/*
 * function to pend on the priority queue to wait for incoming message
 *
 * @param queue: pointer of priority queue
 * @param timeout: timeout for waiting
 *
 * */
inline void *ida_lwip_prioQueuePend(IDA_LWIP_PRIO_QUEUE *queue, u32_t timeout){
	CPU_SR cpu_sr;
	u16_t prio = 0;
	void *msg;

	sys_arch_sem_wait(&queue->act_sem, timeout);
	OS_ENTER_CRITICAL();
	prio = 8*sizeof(int) - 1 - __builtin_clz(queue->prio_field);
	queue->count[prio]--;
	if(queue->count[prio] == 0)
		queue->prio_field &= ~(1 << prio);
	sys_arch_mbox_tryfetch(&queue->mbox[prio], &msg);
	OS_EXIT_CRITICAL();

	return msg;
}


