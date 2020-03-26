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


#ifndef SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_PRIO_QUEUE_H_
#define SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_PRIO_QUEUE_H_

#include "lwip/sys.h"
#include "ida_lwip_queue.h"


#define IDA_LWIP_MBOX_SIZE 10

typedef struct{
	IDA_LWIP_QUEUE *queue[8];
	u8_t prio_field;
	sys_sem_t act_sem;
}IDA_LWIP_PRIO_QUEUE;

void ida_lwip_prioQueueInit(void);
IDA_LWIP_PRIO_QUEUE* ida_lwip_prioQueueCreate(int size);
void ida_lwip_prioQueueDestroy(IDA_LWIP_PRIO_QUEUE* queue);
err_t ida_lwip_prioQueuePut(IDA_LWIP_PRIO_QUEUE *queue, void *msg, u8_t prio);
void *ida_lwip_prioQueuePend(IDA_LWIP_PRIO_QUEUE *queue, u32_t timeout);

#endif /* SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_PRIO_QUEUE_H_ */
