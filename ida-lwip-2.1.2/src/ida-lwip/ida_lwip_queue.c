/*
 * Copyright 2019 TU Braunschweig,
 * 		Institute of Computer and NetworkEngineering (IDA)
 *
 * Contributors: Kai-Bjoern Gemlau (gemlau@ida.ing.tu-bs.de)
 * 				 Nora Sperling
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
*/


#include "ida-lwip/ida_lwip_queue.h"
#include "lwip/memp.h"

LWIP_MEMPOOL_DECLARE(IDA_LWIP_QUEUE_POOL, IDA_LWIP_QUEUE_COUNT, sizeof(IDA_LWIP_QUEUE), "queue pool");

/*
 * initialization of the queues
 * */
void ida_lwip_queue_init(void)
{
	/* Initialize queue pool */
	LWIP_MEMPOOL_INIT(IDA_LWIP_QUEUE_POOL);
}

IDA_LWIP_QUEUE * ida_lwip_queue_alloc(u16_t size)
{
	if (size > IDA_LWIP_QUEUE_SIZE)
		return (IDA_LWIP_QUEUE*) NULL;

	IDA_LWIP_QUEUE* queue = (IDA_LWIP_QUEUE*) LWIP_MEMPOOL_ALLOC(IDA_LWIP_QUEUE_POOL);

	if(queue != (IDA_LWIP_QUEUE*) NULL){
		queue->count = 0;
		queue->head = 0;
		queue->tail = 0;
		queue->max = 0;
		queue->size = size;
		queue->trigger = size;
	}

	return queue;
}

void ida_lwip_queue_free(IDA_LWIP_QUEUE* queue){
	CPU_SR cpu_sr;
	if(queue == (IDA_LWIP_QUEUE*)NULL)
		return;
	OS_ENTER_CRITICAL();
	LWIP_MEMPOOL_FREE(IDA_LWIP_QUEUE_POOL, (void*)queue);
	OS_EXIT_CRITICAL();
}

int ida_lwip_queue_put(IDA_LWIP_QUEUE* queue, void* data){
	CPU_SR cpu_sr;
	if(queue == (IDA_LWIP_QUEUE*)NULL)
		return -1;

	OS_ENTER_CRITICAL();
	if(queue->count >= queue->trigger){
		OS_EXIT_CRITICAL();
		return -1;
	}

	queue->data[queue->head] = (u32_t) data;
	queue->count++;
	if(queue->count > queue->max)
		queue->max = queue->count;

	queue->head = (queue->head + 1) % queue->size;
	OS_EXIT_CRITICAL();

	return 0;
}

void * ida_lwip_queue_get(IDA_LWIP_QUEUE* queue){
	CPU_SR cpu_sr;
	u32_t data;

	if(queue == (IDA_LWIP_QUEUE*)NULL)
		return NULL;

	OS_ENTER_CRITICAL();
	if(queue->count == 0){
		OS_EXIT_CRITICAL();
		return NULL;
	}

	data = queue->data[queue->tail];
	queue->count--;
	queue->tail = (queue->tail + 1) % queue->size;

	OS_EXIT_CRITICAL();
	return (void*) data;
}

void ida_lwip_queue_set_trigger(IDA_LWIP_QUEUE* queue, u16_t trigger){
	CPU_SR cpu_sr;

	if(queue == (IDA_LWIP_QUEUE*)NULL)
		return;

	OS_ENTER_CRITICAL();
	if(trigger <= queue->size)
		queue->trigger = trigger;
	else
		queue->trigger = queue->size;
	OS_EXIT_CRITICAL();
}





