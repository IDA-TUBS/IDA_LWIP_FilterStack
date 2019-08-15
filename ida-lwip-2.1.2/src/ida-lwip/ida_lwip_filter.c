/*
 * ida_lwip_filter.c
 *
 *  Created on: Jun 11, 2019
 *      Author: noras
 */

#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/ip.h"
#include "lwip/sockets.h"
#include "lwip/priv/sockets_priv.h"
#include "lwip/udp.h"
#include "ida-lwip/ida_lwip_filter.h"
#include "ida-lwip/ida_lwip_monitor.h"
#include "ida-lwip/ida_lwip_prio_queue.h"

IDA_LWIP_FILTER_QUEUE ida_filter_queue;

IDA_LWIP_FILTER_MBOX dummy_task_mbox;

PBUF_MONITOR_T *classic_mon;

struct netif *netif_local;				// local save of netif, is needed to call ip4_input

IDA_LWIP_PRIO_QUEUE *inputQueue;
IDA_LWIP_PRIO_QUEUE *outputQueue;

static void _ida_filter_thread(void* p_arg);
static void _ida_filter_tx_thread(void* p_arg);

/*
 * initialization of the 8 mboxes
 * */
void ida_filter_init(struct netif *netif){
	ida_monitor_init();
	ida_lwip_prioQueueInit();

	netif_local = netif;

	/*Initialization of mbox for dummy task*/
	sys_mbox_new(&dummy_task_mbox.mbox, IDA_FILTER_MBOX_SIZE);
	dummy_task_mbox.count = 0;

	classic_mon = ida_monitor_alloc(2);

	/*Initialization of ida_filter_queue*/

	inputQueue = ida_lwip_prioQueueCreate(10);
	outputQueue = ida_lwip_prioQueueCreate(10);

	sys_thread_new("ida_lwip_filter",(void (*)(void*)) _ida_filter_thread, NULL, 512,	TCPIP_THREAD_PRIO - 1);
	sys_thread_new("ida_lwip_tx_filter",(void (*)(void*)) _ida_filter_tx_thread, NULL, 512,	TCPIP_THREAD_PRIO - 2);
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
		return ida_lwip_prioQueuePut(inputQueue,data,prio);
	else
		return ida_lwip_prioQueuePut(outputQueue,data,prio);
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
		p = (struct pbuf*)ida_lwip_prioQueuePend(inputQueue,0);
		err = ip4_input(p, netif_local);
		if(err == ERR_NOTUS){
			if(ida_monitor_check(p,classic_mon) > 0){
				//Todo: Send to classic stack
				sys_mbox_trypost(&dummy_task_mbox.mbox, (void*)p);
				OS_ENTER_CRITICAL();
				dummy_task_mbox.count++;
				OS_EXIT_CRITICAL();
			} else {
				pbuf_free(p);
			}
		}
	}
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
	u8_t prio = 0;
	CPU_SR cpu_sr;

	while(1){
		txReq = (IDA_LWIP_TX_REQ*)ida_lwip_prioQueuePend(outputQueue,0);
		if(txReq != NULL){
			if(txReq->type == UDP){
				struct ida_lwip_sock *sock;
				sock = get_socket(txReq->socket);
				if (sock) {
					struct sockaddr_in *to = (struct sockaddr_in *)txReq->to;
					ip_addr_t addr;
					u16_t remote_port;
					struct pbuf * p;
					p = pbuf_alloc(PBUF_TRANSPORT, 0, PBUF_REF);
					if (p != NULL){
						p->payload = txReq->data;
						p->len = txReq->size;
						if (to) {
							addr.addr = to->sin_addr.s_addr;
							remote_port = lwip_ntohs(to->sin_port);
							sock->err = udp_sendto_if(sock->pcb, p, &addr, remote_port, netif_local);
						} else {
							sock->err = udp_sendto_if(sock->pcb, p, &sock->pcb->remote_ip, sock->pcb->remote_port, netif_local);
						}
					} else {
						sock->err = ERR_MEM;
					}
					sys_sem_signal(sock->sem);
				} else {
					// What should we do here????
					//return -1;
				}

			} else if(txReq->type == RAW){

			} else {

			}
		}
	}
}
