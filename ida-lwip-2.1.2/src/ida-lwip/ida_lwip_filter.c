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

IDA_LWIP_FILTER_QUEUE ida_filter_queue;

struct netif *netif_local;				// local save of netif, is needed to call ip4_input

static void _ida_filter_thread(void* p_arg);

/*
 * initialization of the 8 mboxes
 * */
void ida_filter_init(struct netif *netif){
	netif_local = netif;
	for(int i = 0; i < 8; i++){
		sys_mbox_new(&ida_filter_queue.filter_mbox[i].mbox, IDA_FILTER_MBOX_SIZE);
		ida_filter_queue.filter_mbox[i].count = 0;
	}
	ida_filter_queue.prio_field = 0;
	sys_sem_new(&ida_filter_queue.act_sem,0);
	sys_thread_new("ida_lwip_filter",(void (*)(void*)) _ida_filter_thread, NULL, 512,	TCPIP_THREAD_PRIO - 1);

}

/*
 * function to enqueue pbuf in correct mbox for vlan prio
 *
 * @param p pbuf to enqueue
 * @param prio vlan prio of ethernet packet and therefore prio of message queue, the pbuf will be queued into
 * */
err_t ida_filter_enqueue_pkt(struct pbuf *p, u8_t prio){
	CPU_SR cpu_sr;

	if(prio > 7){
		return ERR_ARG;
	}

	sys_mbox_trypost(&ida_filter_queue.filter_mbox[prio].mbox, (void*)p);

	OS_ENTER_CRITICAL();
	ida_filter_queue.filter_mbox[prio].count++;
	ida_filter_queue.prio_field |= (1 << prio);
	sys_sem_signal(&ida_filter_queue.act_sem);
	OS_EXIT_CRITICAL();

	return ERR_OK;
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
		sys_arch_sem_wait(&ida_filter_queue.act_sem, MBOX_SEM_TIMEOUT);
		OS_ENTER_CRITICAL();
		prio = 8*sizeof(int) - 1 - __builtin_clz(ida_filter_queue.prio_field);
		ida_filter_queue.filter_mbox[prio].count--;
		if(ida_filter_queue.filter_mbox[prio].count == 0)
			ida_filter_queue.prio_field &= ~(1 << prio);
		OS_EXIT_CRITICAL();
		if(sys_arch_mbox_tryfetch(&ida_filter_queue.filter_mbox[prio].mbox, &msg) != SYS_MBOX_EMPTY){
			p = (struct pbuf *)msg;
			err = ip4_input(p, netif_local);
			if(err == ERR_NOTUS){
				//Todo: Send to classic stack
				pbuf_free(p);
			}
			break;
		}
	}
}
