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


#include "lwip/opt.h"
#include "ida-lwip/ida_lwip_igmp.h"
#include "ida_lwip_virtEth.h"
#include "ida_lwip_virtEth_master.h"
#include "lwip/ip.h"
#include "string.h" 	//For memset

ip4_addr_t igmp_groups[IDA_LWIP_MAX_IGMP_GROUPS];

static ip4_addr_t     allsystems;
static ip4_addr_t     allrouters;

/*
 * funtion to initialize the memory pool for the igmp groups queue
 *
 * */
void ida_lwip_igmp_init(void){
	memset(igmp_groups,IPADDR_ANY,IDA_LWIP_MAX_IGMP_GROUPS * sizeof(ip4_addr_t));

	IP4_ADDR(&allsystems, 224, 0, 0, 1);
	IP4_ADDR(&allrouters, 224, 0, 0, 2);
}

/**
 * Search for a group in the netif's igmp group list
 *
 * @param ifp the network interface for which to look
 * @param addr the group ip address to search for
 * @return a struct igmp_group* if the group has been found,
 *         NULL if the group wasn't found.
 */
int ida_lwip_igmp_is_member(const ip4_addr_t *addr)
{
	CPU_SR cpu_sr;
	OS_ENTER_CRITICAL();
	int i = 0;
	for(i = 0; i < IDA_LWIP_MAX_IGMP_GROUPS; i++){
		if(ip4_addr_cmp(&(igmp_groups[i]), addr)){
			OS_EXIT_CRITICAL();
			return i;
		}

	}
	OS_EXIT_CRITICAL();
	return -1;
}


/**
 * @ingroup igmp
 * Join a group on one network interface.
 *
 * @param ifaddr ip address of the network interface which should join a new group
 * @param groupaddr the ip address of the group which to join
 * @return ERR_OK if group was joined on the netif(s), an err_t otherwise
 */
err_t
ida_lwip_igmp_joingroup(const ip4_addr_t *ifaddr, const ip4_addr_t *groupaddr)
{
	err_t err = ERR_VAL; /* no matching interface */
	int firstfree = -1;
	int i = 0;

	/* make sure it is multicast address */
	if(!ip4_addr_ismulticast(groupaddr) || ip4_addr_cmp(groupaddr, &allsystems))
		return ERR_VAL;

	/* First check if we are already in that group */
	for(i = 0; i < IDA_LWIP_MAX_IGMP_GROUPS; i++){
		if(ip4_addr_cmp(&igmp_groups[i],groupaddr)){
			/* We are already group member, exit */
			return ERR_OK;
		}
		if(ip4_addr_cmp(&igmp_groups[i],IP_ADDR_ANY) && firstfree == -1){
			/* If we have found an empty entry, simply keep it in mind to use it later */
			firstfree = i;
		}
	}

	/* We have not found a group so far, so we can join a new one */
	if(firstfree >= 0){
		/* store this in the first free entry of the table */
		igmp_groups[firstfree].addr = groupaddr->addr;

		u32_t mgmtData[2];
		mgmtData[0] = IDA_LWIP_MGMT_IGMP_JOIN;
		mgmtData[1] = groupaddr->addr;

		ida_lwip_virtEth_sendToClassicMgmt(mgmtData,sizeof(mgmtData));

		netif_default->igmp_mac_filter(netif_default, groupaddr, NETIF_ADD_MAC_FILTER);

	} else {
		/* We have not enough memory */
		return ERR_MEM;;
	}
}
