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


#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/ip.h"
#include "lwip/sockets.h"
#include "lwip/priv/sockets_priv.h"
#include "lwip/udp.h"
#include "ida-lwip/ida_lwip_filter.h"
#include "ida-lwip/ida_lwip_queue.h"
#include "ida-lwip/ida_lwip_prio_queue.h"
#include "ida_lwip_virtEth_master.h"

struct netif *netif_local;				// local save of netif, is needed to call ip4_input

static IDA_LWIP_PRIO_QUEUE *_ida_lwip_inputQueue;
static IDA_LWIP_PRIO_QUEUE *_ida_lwip_outputQueue;

static IDA_LWIP_QUEUE *_ida_lwip_classicQueue;
static sys_sem_t	_ida_lwip_classicSem;

#if LWIP_PTP == 1
static IDA_LWIP_QUEUE *_ida_lwip_ptpInputQueue = NULL;
static sys_sem_t	_ida_lwip_ptpInputSem = NULL;
#endif

static void _ida_filter_thread(void* p_arg);
static void _ida_filter_tx_thread(void* p_arg);
static void _ida_filter_classicAdapter(void* p_arg);

extern err_t low_level_output(struct netif *netif, struct pbuf *p);

LWIP_MEMPOOL_DECLARE(TX_PBUF_POOL, 20, sizeof(IDA_LWIP_FILTER_PBUF), "tx Pbuf_Pool");


/*
 * initialization of the 8 mboxes
 * */
void ida_filter_init(struct netif *netif){
	netif_local = netif;

	/*Initialization of mbox for dummy task*/
	_ida_lwip_classicQueue = ida_lwip_queue_alloc(IDA_LWIP_QUEUE_SIZE);
	ida_lwip_queue_set_trigger(_ida_lwip_classicQueue, 4);
	sys_sem_new(&_ida_lwip_classicSem, 0);

#if LWIP_PTP == 1
	sys_sem_new(&_ida_lwip_ptpInputSem, 0);
	_ida_lwip_ptpInputQueue = ida_lwip_queue_alloc(4);
#endif

	/* Initialize tx pbufs */
	LWIP_MEMPOOL_INIT(TX_PBUF_POOL);

	/*Initialization of ida_filter_queue*/

	_ida_lwip_inputQueue = ida_lwip_prioQueueCreate(IDA_LWIP_MBOX_SIZE);
	_ida_lwip_outputQueue = ida_lwip_prioQueueCreate(IDA_LWIP_MBOX_SIZE);

	sys_thread_new("ida_lwip_rx_filter",(void (*)(void*)) _ida_filter_thread, NULL, IDA_LWIP_RX_FILTER_STACK_SIZE,	TCPIP_THREAD_PRIO - 1);
	sys_thread_new("ida_lwip_tx_filter",(void (*)(void*)) _ida_filter_tx_thread, NULL, IDA_LWIP_TX_FILTER_STACK_SIZE,	TCPIP_THREAD_PRIO - 2);
}

/*
 * Initialize the virtEth interface, sync with slave core and start the classic adapter
 * this must be called when the ethernet interface is ready
 */
void ida_filter_init_classicAdapter(void){
	sys_thread_new("ida_lwip_classicAdapter", (void (*)(void*)) _ida_filter_classicAdapter, NULL, IDA_LWIP_CLASSIC_ADAPTER_STACK_SIZE,	OS_LOWEST_PRIO - 10);
}

/*
 * function to enqueue pbuf in correct mbox for vlan prio
 *
 * @param void* data to enqueue
 * @param prio vlan prio of ethernet packet and therefore prio of message queue, the pbuf will be queued into
 * @param direction (1=RX 0=TX)
 * */
err_t ida_filter_enqueue_pkt(void *data, u8_t prio, u8_t direction){
	if(direction)
		return ida_lwip_prioQueuePut(_ida_lwip_inputQueue,data,prio);
	else
		return ida_lwip_prioQueuePut(_ida_lwip_outputQueue,data,prio);
}

/*
 * thread to process packet/ pbufs that were queued into message queues
 *
 * @param p_arg currently unused
 * */
static void _ida_filter_thread(void* p_arg){
	void* msg =  NULL;
	struct pbuf *p;
	err_t err;
	u8_t prio = 0;
	CPU_SR cpu_sr;

	while(1){
		p = (struct pbuf*)ida_lwip_prioQueuePend(_ida_lwip_inputQueue,0);
		err = ip4_input(p, netif_local);
		if(err == ERR_NOTUS){
			ida_filter_sendToClassic(p);
		}
	}
}

void ida_filter_sendToClassic(struct pbuf *p){
	/** Restore original payload and length fields */
	p->payload = p->payload_orig;
	p->tot_len = p->tot_len_orig;
	p->len = p->tot_len;

	if(ida_lwip_queue_put(_ida_lwip_classicQueue,(void*) p) != -1){
		sys_sem_signal(&_ida_lwip_classicSem);
	} else {
		pbuf_free(p);
	}
}

static void _ida_lwip_tx_pbuf_free(struct pbuf *p)
{
	CPU_SR cpu_sr;
	OS_ENTER_CRITICAL();
	IDA_LWIP_FILTER_PBUF* tx_pbuf = (IDA_LWIP_FILTER_PBUF*)p;
	sys_sem_signal(&tx_pbuf->tx_complete_sem);
	LWIP_MEMPOOL_FREE(TX_PBUF_POOL, tx_pbuf);
	OS_EXIT_CRITICAL();
}

static void _ida_filter_process_udp(IDA_LWIP_TX_REQ* txReq){
	struct ida_lwip_sock *sock;
	struct sockaddr_in *to;
	ip_addr_t addr;
	u16_t remote_port;
	IDA_LWIP_FILTER_PBUF* tx_pbuf;
	struct pbuf* p;
	sock = get_socket(txReq->socket);
	if (sock == NULL) {
		txReq->err = ERR_VAL;
		sys_sem_signal(&txReq->txCompleteSem);
		return;
	}
	to = (struct sockaddr_in *)txReq->to;

	tx_pbuf = (IDA_LWIP_FILTER_PBUF*)LWIP_MEMPOOL_ALLOC(TX_PBUF_POOL);
	if(tx_pbuf == NULL){
		txReq->err = ERR_MEM;
		sys_sem_signal(&txReq->txCompleteSem);
		return;
	}
	tx_pbuf->tx_complete_sem = txReq->txCompleteSem;
	tx_pbuf->p.custom_free_function = _ida_lwip_tx_pbuf_free;
	p = pbuf_alloced_custom(PBUF_TRANSPORT,0,PBUF_REF,&tx_pbuf->p,NULL,1500);
	if ( p == NULL){
		txReq->err = ERR_MEM;
		sys_sem_signal(&txReq->txCompleteSem);
		return;
	}
	p->payload = txReq->data;
	p->len = txReq->size;
	p->tot_len = p->len;
	p->ethPrio = ida_lwip_get_socket_prio(sock->id);
	if (to) {
		addr.addr = to->sin_addr.s_addr;
		remote_port = lwip_ntohs(to->sin_port);
		txReq->err = udp_sendto_if(sock->pcb, p, &addr, remote_port, netif_local);
	} else {
		txReq->err = udp_sendto_if(sock->pcb, p, &sock->pcb->remote_ip, sock->pcb->remote_port, netif_local);
	}
	if(txReq->err != ERR_OK)
		sys_sem_signal(&txReq->txCompleteSem);
}

/*
 * thread to process TX Requests
 *
 * @param p_arg currently unused
 * */
static void _ida_filter_tx_thread(void* p_arg){
	void* msg =  NULL;
	IDA_LWIP_TX_REQ *txReq;
	err_t err;


	while(1){
		txReq = (IDA_LWIP_TX_REQ*)ida_lwip_prioQueuePend(_ida_lwip_outputQueue,0);
		if(txReq != NULL){
			if(txReq->type == UDP){
				_ida_filter_process_udp(txReq);
			} else if(txReq->type == RAW){
				struct pbuf * p;
				p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_REF);
				if (p != NULL){
					p->payload = txReq->data;
					p->len = txReq->size;
					txReq->err = netif_local->linkoutput(netif_local, p);
				} else {
					txReq->err = ERR_MEM;
				}
				sys_sem_signal(&txReq->txCompleteSem);
			} else {
				//todo: ??
			}
		}
	}
}

/*
 * function to send raw data
 *
 * @param data: data to send
 * @param size: size of data
 * @param completeSem: semaphore to signal the transmission was completed
 *
 * @return size of send data
 * */
ssize_t ida_lwip_send_raw(void *data, size_t size, sys_sem_t *completeSem)
{
  if (size > IDA_LWIP_MAX_MSS || data == NULL) {
	  return -1;
  }

  if (!sys_sem_valid(completeSem))
	  return -1;

  IDA_LWIP_TX_REQ txReq;
  txReq.type = RAW;
  txReq.data = data;
  txReq.size = size;
  txReq.err = ERR_OK;
  txReq.txCompleteSem = *completeSem;

  if(ida_filter_enqueue_pkt((void*)&txReq, 0, 0) != ERR_OK)
	  sys_sem_signal(&txReq.txCompleteSem);

  sys_arch_sem_wait(&txReq.txCompleteSem, 0);

  return txReq.err == ERR_OK ? size : -1;
}

/*
 * classic adapter function
 *
 * @param p_arg currently unused
 * */
static void _ida_filter_classicAdapter(void* p_arg){
	sys_sem_t txCompleteSem;
	if(sys_sem_new(&txCompleteSem,0) != ERR_OK){
		return;
	}

	ida_lwip_virtEth_master_init();

	while(1){
		u32_t res = sys_arch_sem_wait(&_ida_lwip_classicSem,1);
		if(res == SYS_ARCH_TIMEOUT){
			/** Check if we need to send something from the classic stack */
			ida_lwip_virtEth_receiveFromClassic(txCompleteSem);

		} else {
			/** We received something that we need to pass to the the classic stack **/
			struct pbuf *p = (struct pbuf*)ida_lwip_queue_get(_ida_lwip_classicQueue);
			if(p != NULL){
				ida_lwip_virtEth_sendToClassic(p);
				pbuf_free(p);
			}
		}
	}
}

#if LWIP_PTP == 1
/**
 * Read packet from ptp queue
 * @param u32_t timeout: Timeout for waiting
 */
struct pbuf* ida_filter_receivePtp(u32_t timeout){

	u32_t waiting = sys_arch_sem_wait(&_ida_lwip_ptpInputSem, timeout);
	if(waiting == SYS_ARCH_TIMEOUT)
		return NULL;

	return (struct pbuf*)ida_lwip_queue_get(_ida_lwip_ptpInputQueue);
}

int ida_filter_enqueu_ptp_rx(struct pbuf *p){
	if(ida_lwip_queue_put(_ida_lwip_ptpInputQueue, (void*)p) == 0){
		sys_sem_signal(&_ida_lwip_ptpInputSem);
		return 0;
	} else {
		return -1;
	}
}
#endif
