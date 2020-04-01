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


#ifndef __NETIF_IDA_LWIP_VIRTETH_H__
#define __NETIF_IDA_LWIP_VIRTETH_H__

#include "lwip/netif.h"
#include "netif/etharp.h"
#include "lwip/sys.h"

#define IDA_LWIP_MEM_MGMT_BASE			0xFFFC0000
#define IDA_LWIP_MEM_QUEUE_SIZE			32

#define IDA_LWIP_MEM_FROM_CLASSIC_BASE			0xFFFD0000
#define	IDA_LWIP_MEM_FROM_CLASSIC_ENTRY_SIZE 	1502
#define IDA_LWIP_MEM_FROM_CLASSIC_SIZE			(IDA_LWIP_MEM_FROM_CLASSIC_ENTRY_SIZE*IDA_LWIP_MEM_QUEUE_SIZE)

#define IDA_LWIP_MEM_TO_CLASSIC_BASE			0xFFFE0000
#define	IDA_LWIP_MEM_TO_CLASSIC_ENTRY_SIZE 		1502
#define IDA_LWIP_MEM_TO_CLASSIC_SIZE			(IDA_LWIP_MEM_TO_CLASSIC_ENTRY_SIZE*IDA_LWIP_MEM_QUEUE_SIZE)

#define IDA_LWIP_MEM_MSG_TYPE_UNDEF		0
#define IDA_LWIP_MEM_MSG_TYPE_DATA		1
#define IDA_LWIP_MEM_MSG_TYPE_MGMT		2

#define IDA_LWIP_MGMT_IGMP_JOIN			0
#define IDA_LWIP_MGMT_IGMP_LEAVE		1

#define CEIL_ALIGN(x) 	x%sizeof(u32_t)==0 ? (x) : ((x/sizeof(u32_t))+sizeof(u32_t))

typedef struct __attribute__((packed, aligned(8))){
	u32_t 	size;
	u32_t 	ref;
	u32_t	type;
} IDA_LWIP_IPI_QUEUE_ENTRY;

typedef struct __attribute__((packed, aligned(8))){
	u32_t lock;
	u8_t	head;
	u8_t	tail;
	u8_t	used;
	u8_t	maxUsed;
	IDA_LWIP_IPI_QUEUE_ENTRY	data[IDA_LWIP_MEM_QUEUE_SIZE];
} IDA_LWIP_IPI_QUEUE;

typedef struct __attribute__((packed)){
	u32_t					ready[2];
	IDA_LWIP_IPI_QUEUE		freeRxBuffers;
	IDA_LWIP_IPI_QUEUE		freeTxBuffers;	/* free buffers that can be filled by classic stack */
	IDA_LWIP_IPI_QUEUE		rxBuffers;		/* RX Packets to classic stack */
	IDA_LWIP_IPI_QUEUE		txBuffers;		/* TX Packets from classic stack */
} SHARED_MEMORY_MGMT;

void spin_lock(u32_t *l);
void spin_unlock(u32_t *l);
u8_t ida_lwip_virtEth_queuePut(IDA_LWIP_IPI_QUEUE *queue, IDA_LWIP_IPI_QUEUE_ENTRY *entry);
u8_t ida_lwip_virtEth_queueGet(IDA_LWIP_IPI_QUEUE *queue, IDA_LWIP_IPI_QUEUE_ENTRY *entry);
u8_t ida_lwip_virtEth_queueInit(IDA_LWIP_IPI_QUEUE *queue);

#endif /* __NETIF_IDA_LWIP_VIRTETH_H__ */
