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
void ida_lwip_prioQueueDestroy(IDA_LWIP_PRIO_QUEUE* prioQueue){
	if(prioQueue == (IDA_LWIP_PRIO_QUEUE*)NULL)
		return;

	for(int i = 0; i < 8; i++){
		if(prioQueue->queue[i] != (IDA_LWIP_QUEUE*)0)
			ida_lwip_queue_free(prioQueue->queue[i]);
	}

	if(prioQueue->act_sem != NULL)
		sys_sem_free(&prioQueue->act_sem);

	LWIP_MEMPOOL_FREE(PRIO_QUEUE_POOL, prioQueue);
}

/*
 * function to create the prority queue
 *
 * @param size: size of the message boxes for each priority
 *
 * @return priority queue
 * */
IDA_LWIP_PRIO_QUEUE* ida_lwip_prioQueueCreate(int size){
	IDA_LWIP_PRIO_QUEUE* prioQueue = (IDA_LWIP_PRIO_QUEUE*)LWIP_MEMPOOL_ALLOC(PRIO_QUEUE_POOL);
	IDA_LWIP_QUEUE *queue;
	if(prioQueue == (IDA_LWIP_PRIO_QUEUE*)NULL)
		return (IDA_LWIP_PRIO_QUEUE*)NULL;

	memset(prioQueue,0,sizeof(IDA_LWIP_PRIO_QUEUE));

	for(int i = 0; i < 8; i++){
		queue = ida_lwip_queue_alloc(size);
		if(queue == (IDA_LWIP_QUEUE*)0){
			ida_lwip_prioQueueDestroy(prioQueue);
			return NULL;
		}
		prioQueue->queue[i] = queue;
	}
	prioQueue->prio_field = 0;
	if(sys_sem_new(&prioQueue->act_sem,0) != ERR_OK){
		ida_lwip_prioQueueDestroy(prioQueue);
		return NULL;
	}

	return prioQueue;
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
inline err_t ida_lwip_prioQueuePut(IDA_LWIP_PRIO_QUEUE *prioQueue, void *msg, u8_t prio){
	CPU_SR cpu_sr;

	if(prio > 7 || prioQueue == (IDA_LWIP_PRIO_QUEUE*)NULL){
		return ERR_ARG;
	}
	OS_ENTER_CRITICAL();

	if(ida_lwip_queue_put(prioQueue->queue[prio], msg) == -1){
		OS_EXIT_CRITICAL();
		return ERR_MEM;
	}

	prioQueue->prio_field |= (1 << prio);
	sys_sem_signal(&prioQueue->act_sem);
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
inline void *ida_lwip_prioQueuePend(IDA_LWIP_PRIO_QUEUE *prioQueue, u32_t timeout){
	CPU_SR cpu_sr;
	u16_t prio = 0;
	void *msg;

	sys_arch_sem_wait(&prioQueue->act_sem, timeout);
	OS_ENTER_CRITICAL();
	prio = 8*sizeof(int) - 1 - __builtin_clz(prioQueue->prio_field);
	msg = ida_lwip_queue_get(prioQueue->queue[prio]);
	if(prioQueue->queue[prio]->count == 0)
		prioQueue->prio_field &= ~(1 << prio);
	OS_EXIT_CRITICAL();

	return msg;
}


