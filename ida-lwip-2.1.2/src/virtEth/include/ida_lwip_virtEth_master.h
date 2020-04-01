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


#ifndef __NETIF_IDA_LWIP_VIRTETH_MASTER_H__
#define __NETIF_IDA_LWIP_VIRTETH_MASTER_H__

#include "lwip/netif.h"
#include "netif/etharp.h"
#include "lwip/sys.h"
#include "ida_lwip_virtEth.h"

void ida_lwip_virtEth_master_init(void);
void ida_lwip_virtEth_receiveFromClassic(sys_sem_t txCompleteSem);
void ida_lwip_virtEth_sendToClassic(struct pbuf* p);
void ida_lwip_virtEth_sendToClassicMgmt(void *mgmtData, size_t size);

#endif /* __NETIF_IDA_LWIP_VIRTETH_H__ */
