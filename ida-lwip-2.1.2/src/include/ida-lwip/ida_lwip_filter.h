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


#ifndef _IDA_LWIP_IDA_LWIP_FILTER_H_
#define _IDA_LWIP_IDA_LWIP_FILTER_H_

#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/ip.h"

#ifndef IDA_LWIP_TX_FILTER_STACK_SIZE
#define IDA_LWIP_TX_FILTER_STACK_SIZE 	256
#endif

#ifndef IDA_LWIP_RX_FILTER_STACK_SIZE
#define IDA_LWIP_RX_FILTER_STACK_SIZE 	256
#endif

#ifndef IDA_LWIP_CLASSIC_ADAPTER_STACK_SIZE
#define IDA_LWIP_CLASSIC_ADAPTER_STACK_SIZE 	256
#endif


typedef struct IDA_LWIP_FILTER_PBUF
{
   struct pbuf_custom p;
   sys_sem_t tx_complete_sem;
} IDA_LWIP_FILTER_PBUF;

typedef struct{
	enum {UDP, RAW} type;
	void *data;
	size_t size;
	int socket;
	void *to;
	int err;
	sys_sem_t txCompleteSem;
}IDA_LWIP_TX_REQ;

void ida_filter_sendToClassic(struct pbuf *p);
err_t ida_filter_enqueue_pkt(void *data, u8_t prio, u8_t direction);
ssize_t ida_lwip_send_raw(void *data, size_t size, sys_sem_t *completeSem);
void ida_filter_init(struct netif *netif);


#endif /* _IDA_LWIP_IDA_LWIP_FILTER_H_ */
