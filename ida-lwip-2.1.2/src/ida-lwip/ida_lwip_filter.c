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
#include "ida-lwip/ida_lwip_filter.h"
#include "ida-lwip/ida_lwip_monitor.h"
#include "ida-lwip/ida_lwip_prio_queue.h"

IDA_LWIP_FILTER_QUEUE ida_filter_queue;

IDA_LWIP_FILTER_MBOX dummy_task_mbox;

PBUF_MONITOR_T *classic_mon;

struct netif *netif_local;				// local save of netif, is needed to call ip4_input

IDA_LWIP_PRIO_QUEUE *inputQueue;

static void _ida_filter_thread(void* p_arg);

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

	sys_thread_new("ida_lwip_filter",(void (*)(void*)) _ida_filter_thread, NULL, 512,	TCPIP_THREAD_PRIO - 1);

}

/*
 * function to enqueue pbuf in correct mbox for vlan prio
 *
 * @param p pbuf to enqueue
 * @param prio vlan prio of ethernet packet and therefore prio of message queue, the pbuf will be queued into
 * */
err_t ida_filter_enqueue_pkt(struct pbuf *p, u8_t prio){
	return ida_lwip_prioQueuePut(inputQueue,(void*)p,prio);
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
