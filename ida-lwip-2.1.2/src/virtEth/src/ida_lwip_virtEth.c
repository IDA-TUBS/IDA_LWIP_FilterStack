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

void spin_lock(volatile u32_t *l) {
  while (__atomic_test_and_set(l, __ATOMIC_ACQUIRE));;
}

void spin_unlock(volatile u32_t *l){
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


