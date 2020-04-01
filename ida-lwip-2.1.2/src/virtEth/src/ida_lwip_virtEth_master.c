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

static SHARED_MEMORY_MGMT  *_ida_lwip_sharedMem;

static u8_t *_ida_lwip_mem_from_classic;
static u8_t *_ida_lwip_mem_to_classic;

extern XIpiPsu_Config XIpiPsu_ConfigTable[];

void ida_lwip_virtEth_master_init(void){
	IDA_LWIP_IPI_QUEUE_ENTRY entry;

	_ida_lwip_sharedMem = (SHARED_MEMORY_MGMT*)IDA_LWIP_MEM_MGMT_BASE;
	memset((void*)_ida_lwip_sharedMem,0,sizeof(SHARED_MEMORY_MGMT));
	_ida_lwip_mem_from_classic = (u8_t*)IDA_LWIP_MEM_FROM_CLASSIC_BASE;
	_ida_lwip_mem_to_classic   = (u8_t*)IDA_LWIP_MEM_TO_CLASSIC_BASE;

	/* set up tx buffer */
	memset((void*)_ida_lwip_mem_from_classic,0,IDA_LWIP_MEM_FROM_CLASSIC_SIZE);
	for(int i = 0; i < IDA_LWIP_MEM_QUEUE_SIZE; i++){
		/* create entry for queue */
		entry.ref = (u32_t)i;
		entry.size = IDA_LWIP_MEM_FROM_CLASSIC_ENTRY_SIZE;
		entry.type = IDA_LWIP_MEM_MSG_TYPE_UNDEF;
		/* put in txFree Queue */
		ida_lwip_virtEth_queuePut(&_ida_lwip_sharedMem->freeTxBuffers,&entry);
	}

	XIpiPsu_CfgInitialize(&IPIInstance, &XIpiPsu_ConfigTable[0], (UINTPTR) XIpiPsu_ConfigTable[0].BaseAddress);

	_ida_lwip_sharedMem->ready[0] = 1;
	while(_ida_lwip_sharedMem->ready[1] == 0);;
}

void ida_lwip_virtEth_receiveFromClassic(sys_sem_t txCompleteSem){
	IDA_LWIP_IPI_QUEUE_ENTRY entry;
	u8_t res = 0;
	do{
		res = ida_lwip_virtEth_queueGet(&_ida_lwip_sharedMem->txBuffers, &entry);
		if(res == 1){
			void *data = (void*)(IDA_LWIP_MEM_FROM_CLASSIC_BASE + (entry.ref * IDA_LWIP_MEM_FROM_CLASSIC_ENTRY_SIZE));
			if(entry.type == IDA_LWIP_MEM_MSG_TYPE_DATA){
				ida_lwip_send_raw(data, entry.size, &txCompleteSem);
			} else if(entry.type == IDA_LWIP_MEM_MSG_TYPE_MGMT){
				/* handle management message here */
				//Todo: Not implemented yet
			}
			ida_lwip_virtEth_queuePut(&_ida_lwip_sharedMem->freeTxBuffers,&entry);
		}
	}while(res == 1);
}

void ida_lwip_virtEth_sendToClassic(struct pbuf* p){
	IDA_LWIP_IPI_QUEUE_ENTRY entry;
	u8_t res = ida_lwip_virtEth_queueGet(&_ida_lwip_sharedMem->freeRxBuffers, &entry);
	if(res == 1){
		void *data = (void*)(IDA_LWIP_MEM_TO_CLASSIC_BASE + (entry.ref * IDA_LWIP_MEM_TO_CLASSIC_ENTRY_SIZE));
		pbuf_copy_partial(p,data,p->tot_len,0);
		entry.size = p->tot_len;
		entry.type = IDA_LWIP_MEM_MSG_TYPE_DATA;
		ida_lwip_virtEth_queuePut(&_ida_lwip_sharedMem->rxBuffers, &entry);
	}
	XIpiPsu_TriggerIpi(&IPIInstance, XIpiPsu_ConfigTable[0].TargetList[0].Mask);
}

void ida_lwip_virtEth_sendToClassicMgmt(void *mgmtData, size_t size){
	IDA_LWIP_IPI_QUEUE_ENTRY entry;

	if(size > IDA_LWIP_MEM_TO_CLASSIC_ENTRY_SIZE)
		return;

	u8_t res = ida_lwip_virtEth_queueGet(&_ida_lwip_sharedMem->freeRxBuffers, &entry);
	if(res == 1){
		void *data = (void*)(IDA_LWIP_MEM_TO_CLASSIC_BASE + (entry.ref * IDA_LWIP_MEM_TO_CLASSIC_ENTRY_SIZE));
		memcpy(data,mgmtData,size);
		entry.size = size;
		entry.type = IDA_LWIP_MEM_MSG_TYPE_MGMT;
		ida_lwip_virtEth_queuePut(&_ida_lwip_sharedMem->rxBuffers, &entry);
	}
	XIpiPsu_TriggerIpi(&IPIInstance, XIpiPsu_ConfigTable[0].TargetList[0].Mask);
}


