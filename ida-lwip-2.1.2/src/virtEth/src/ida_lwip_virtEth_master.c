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

#include "netif/etharp.h"
#include "ida_lwip_virtEth_master.h"
#include "ida-lwip/ida_lwip_filter.h"
#include "xipipsu.h"

XIpiPsu IPIInstance;

SHARED_MEMORY_MGMT  *sharedMem;

u8_t *txBuffer;
IPI_BUFFER_DESCR txBufferDescriptors[TX_BUFFER_ENTRY_COUNT];
IPI_BUFFER_DESCR *txBufferDescrFree;

void _ida_lwip_virtEth_master_fillTxQueue(void){
	IDA_LWIP_IPI_QUEUE_ENTRY entry;
	/* As long as we have unused buffers */
	while(txBufferDescrFree){
		/* create entry for queue */
		entry.data = txBufferDescrFree->data;
		entry.size = TX_BUFFER_ENTRY_SIZE;
		entry.ref = (void*)txBufferDescrFree;
		/* put in txFree Queue */
		if(ida_lwip_virtEth_queuePut(&sharedMem->freeTxBuffers,&entry) == 1){
			/* if it is successfull, go on with next descriptor */
			txBufferDescrFree = txBufferDescrFree->next;
		} else {
			/* if queue is full: return */
			return;
		}
	}
}

void ida_lwip_virtEth_master_init(void){
	XIpiPsu_Config *ipiConfig;
	sharedMem = (SHARED_MEMORY_MGMT*)IDA_LWIP_MEM_FROM_CLASSIC_BASE;
	memset((void*)sharedMem,0,sizeof(SHARED_MEMORY_MGMT));
	txBuffer = (u8_t*)TX_BUFFER_BASE;

	/* set up tx buffer */
	memset((void*)txBuffer,0,TX_BUFFER_ENTRY_COUNT*TX_BUFFER_ENTRY_SIZE);
	for(int i = 0; i < TX_BUFFER_ENTRY_COUNT; i++){
		txBufferDescriptors[i].data = (void*)&txBuffer[i*TX_BUFFER_ENTRY_SIZE];
		if(i < TX_BUFFER_ENTRY_COUNT - 1)
			txBufferDescriptors[i].next = &txBufferDescriptors[i+1];
		else
			txBufferDescriptors[i].next = NULL;
	}
	txBufferDescrFree = txBufferDescriptors;
	_ida_lwip_virtEth_master_fillTxQueue();

	ipiConfig = XIpiPsu_LookupConfig(0);
	XIpiPsu_CfgInitialize(&IPIInstance, ipiConfig, (UINTPTR) ipiConfig->BaseAddress);
}

void ida_lwip_virtEth_receiveFromClassic(sys_sem_t txCompleteSem){
	IDA_LWIP_IPI_QUEUE_ENTRY entry;
	IPI_BUFFER_DESCR *old;
	u8_t res = ida_lwip_virtEth_queueGet(&sharedMem->txBuffers, &entry);
	if(res == 1){
		ida_lwip_send_raw(entry.data, entry.size, &txCompleteSem);
		old = txBufferDescrFree;
		txBufferDescrFree = (IPI_BUFFER_DESCR*)entry.ref;
		txBufferDescrFree->next = old;
		_ida_lwip_virtEth_master_fillTxQueue();
	}
}

void ida_lwip_virtEth_sendToClassic(struct pbuf* p){
	IDA_LWIP_IPI_QUEUE_ENTRY entry;
	u8_t res = ida_lwip_virtEth_queueGet(&sharedMem->freeRxBuffers, &entry);
	if(res == 1){
		pbuf_copy_partial(p,(void*)entry.data,p->tot_len,0);
		entry.size = p->tot_len;
		ida_lwip_virtEth_queuePut(&sharedMem->rxBuffers, &entry);
	}
	//Trigger IPI
}



