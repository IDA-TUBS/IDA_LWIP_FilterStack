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
#define IDA_LWIP_QUEUE_COUNT	64
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
