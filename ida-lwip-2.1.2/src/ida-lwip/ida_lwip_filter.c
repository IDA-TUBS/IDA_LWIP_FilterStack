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

static sys_mbox_t ida_filter_mbox[8];	// 8 mboxes for each vlan prio
sys_sem_t mbox_sem;						// semaphore to signal that a packet was queued in one of the 8 mboxes
u8_t mbox_status[8];					// packet status of mboxes:
										// 2 packets of prio 1, rest empty -> mbox_status = {0, 2, 0, 0, 0, 0, 0, 0}

struct netif *netif_local;				// local save of netif, is needed to call ip4_input


/*
 * just a way of setting the local netif variable
 * */
void ida_filter_set_netif(struct netif *netif){
	netif_local = netif;
}

/*
 * initialization of the 8 mboxes
 * */
void ida_filter_mbox_init(){

	for(int i = 0; i < 8; i++){
		sys_mbox_new(&ida_filter_mbox[i], IDA_FILTER_MBOX_SIZE);
	}
}

/*
 * function to enqueue pbuf in correct mbox for vlan prio
 *
 * @param p pbuf to enqueue
 * @param prio vlan prio of ethernet packet and therefore prio of message queue, the pbuf will be queued into
 * */
err_t ida_filter_enqueue_pkt(struct pbuf *p, u8_t prio){

	if(prio > 7){
		return ERR_ARG;
	}

	sys_mbox_trypost(&ida_filter_mbox[prio], (void*)p);

	mbox_status[prio]++;
	sys_sem_signal(&mbox_sem);

	return ERR_OK;
}

/*
 * thread to process packet/ pbufs that were queued into message queues
 *
 * @param p_arg currently unused
 * */

void ida_filter_thread(void* p_arg){
	void* msg =  NULL;
	struct pbuf *p;

	ida_filter_mbox_init();

	while(1){
		sys_arch_sem_wait(&mbox_sem, MBOX_SEM_TIMEOUT);

		for(int i; i < 8; i++){
			while(mbox_status[i] > 0){
				sys_arch_mbox_fetch(&ida_filter_mbox[i], &msg, MBOX_SEM_TIMEOUT);
				p = (struct pbuf *)msg;
				ip4_input(p, netif_local);		//need to get the netif from somewhere --> ida_filter_set_netif ??
				mbox_status[i]--;

				/* Nach sem_wait können mehr Pakete abgearbeiten werden,
				 * ohne dass die Semaphore heruntergezählt wird.
				 * -->Problem?										*/
			}
		}

	}
}
