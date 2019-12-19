

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
#include "ida_lwip_virtEth_slave.h"
#include "xipipsu.h"

#include "ucos_int.h"

/* Define those to better describe your network interface. */
#define IFNAME0 'v'
#define IFNAME1 'e'

struct netif *NetIf;
XIpiPsu IPIInstance;

SHARED_MEMORY_MGMT  *_ida_lwip_sharedMem;

sys_sem_t rxSemaphore;


static RX_PBUF_T rxPbufStorage[IDA_LWIP_MEM_QUEUE_SIZE];

static u8_t *_ida_lwip_mem_from_classic;
static u8_t *_ida_lwip_mem_to_classic;

/*
 * low_level_output():
 *
 * Should do the actual transmission of the packet. The packet is
 * contained in the pbuf that is passed to the function. This pbuf
 * might be chained.
 *
 */

static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
	IDA_LWIP_IPI_QUEUE_ENTRY entry;
	SYS_ARCH_DECL_PROTECT(lev);

	SYS_ARCH_PROTECT(lev);

	if(ida_lwip_virtEth_queueGet(&_ida_lwip_sharedMem->freeTxBuffers,&entry) == 0){
		SYS_ARCH_UNPROTECT(lev);
		pbuf_free(p);
#if LINK_STATS
		lwip_stats.link.drop++;
#endif
		return ERR_MEM;
	}
#if LINK_STATS
		lwip_stats.link.xmit++;
#endif
	void* data = (void*)(IDA_LWIP_MEM_FROM_CLASSIC_BASE + entry.ref * IDA_LWIP_MEM_FROM_CLASSIC_ENTRY_SIZE);
	pbuf_copy_partial(p,data,p->tot_len,0);
	entry.size = p->tot_len;
	ida_lwip_virtEth_queuePut(&_ida_lwip_sharedMem->txBuffers,&entry);


	SYS_ARCH_UNPROTECT(lev);
	return ERR_OK;
}

static void _ida_rx_pbuf_free(struct pbuf *p)
{
	CPU_SR cpu_sr;
	RX_PBUF_T* rx_pbuf = (RX_PBUF_T*)p;

	OS_ENTER_CRITICAL();
	IDA_LWIP_IPI_QUEUE_ENTRY entry;
	entry.size = 0;
	entry.ref = rx_pbuf->id;
	ida_lwip_virtEth_queuePut(&_ida_lwip_sharedMem->freeRxBuffers,&entry);
	OS_EXIT_CRITICAL();
}

/*
 * low_level_input():
 *
 * Should allocate a pbuf and transfer the bytes of the incoming
 * packet from the interface into the pbuf.
 *
 */
static struct pbuf * low_level_input(struct netif *netif)
{
	struct pbuf *p;
	IDA_LWIP_IPI_QUEUE_ENTRY entry;
	RX_PBUF_T* rx_pbuf;

	u8_t res = ida_lwip_virtEth_queueGet(&_ida_lwip_sharedMem->rxBuffers, &entry);
	if(res == 0){
		return NULL;
	}
	rx_pbuf = &rxPbufStorage[entry.ref];

	p = pbuf_alloced_custom(PBUF_RAW,
			entry.size,
			PBUF_REF,
			&rx_pbuf->p,
			rx_pbuf->data,
			1502);

	return p;
}

s32_t ida_lwip_virtEth_slave_input(struct netif *netif)
{
	struct eth_hdr *ethhdr;
	struct pbuf *p;
	SYS_ARCH_DECL_PROTECT(lev);

	/* move received packet into a new pbuf */
	SYS_ARCH_PROTECT(lev);
	p = low_level_input(netif);
	SYS_ARCH_UNPROTECT(lev);

	/* no packet could be read, silently ignore this */
	if (p == NULL) {
		return 0;
	}

	/* points to packet payload, which starts with an Ethernet header */
	ethhdr = p->payload;

#if LINK_STATS
	lwip_stats.link.recv++;
#endif /* LINK_STATS */

	switch (htons(ethhdr->type)) {
		/* IP or ARP packet? */
		case ETHTYPE_IP:
		case ETHTYPE_ARP:
#if LWIP_IPV6
		/*IPv6 Packet?*/
		case ETHTYPE_IPV6:
#endif
#if PPPOE_SUPPORT
			/* PPPoE packet? */
		case ETHTYPE_PPPOEDISC:
		case ETHTYPE_PPPOE:
#endif /* PPPOE_SUPPORT */
			/* full packet send to tcpip_thread to process */
			if (netif->input(p, netif) != ERR_OK) {
				LWIP_DEBUGF(NETIF_DEBUG, ("xemacpsif_input: IP input error\r\n"));
				pbuf_free(p);
				p = NULL;
			}
			break;

		default:
			pbuf_free(p);
			p = NULL;
			break;
	}

	return 1;
}

void ida_lwip_virtEth_rxTask(void *p_args){
	while (1) {
		/* sleep until there are packets to process
		 * This semaphore is set by the packet receive interrupt
		 * routine.
		 */
		sys_sem_wait(&rxSemaphore);

		/* move all received packets to lwIP */
		while(ida_lwip_virtEth_slave_input(NetIf) > 0);;
	}
}

void ida_lwip_virtEth_irq_handler(void *p_args){
	u32 IpiSrcMask; /**< Holds the IPI status register value */
	IpiSrcMask = XIpiPsu_GetInterruptStatus(&IPIInstance);
	sys_sem_signal(&rxSemaphore);
	XIpiPsu_ClearInterruptStatus(&IPIInstance,IpiSrcMask);
}

extern XIpiPsu_Config XIpiPsu_ConfigTable[XPAR_XIPIPSU_NUM_INSTANCES];

static err_t low_level_init(struct netif *netif)
{
	NetIf = netif;	/* do we need this? */
	UINTPTR mac_address = (UINTPTR)(netif->state);
	RX_PBUF_T* rx_pbuf;
	IDA_LWIP_IPI_QUEUE_ENTRY entry;

	XIpiPsu_Config *ipiConfig;
	_ida_lwip_sharedMem = (SHARED_MEMORY_MGMT*)IDA_LWIP_MEM_MGMT_BASE;
	_ida_lwip_mem_from_classic = (u8_t*)IDA_LWIP_MEM_FROM_CLASSIC_BASE;
	_ida_lwip_mem_to_classic   = (u8_t*)IDA_LWIP_MEM_TO_CLASSIC_BASE;

	//LWIP_MEMPOOL_INIT(RX_POOL);

	/* set up tx buffer */
//	memset((void*)_ida_lwip_mem_to_classic,0,IDA_LWIP_MEM_TO_CLASSIC_SIZE);

	for(int i = 0; i < IDA_LWIP_MEM_QUEUE_SIZE; i++){
		void *data = (void*)(IDA_LWIP_MEM_TO_CLASSIC_BASE + i * IDA_LWIP_MEM_TO_CLASSIC_ENTRY_SIZE);
		rx_pbuf = &rxPbufStorage[i];
		rx_pbuf->p.custom_free_function = _ida_rx_pbuf_free;
		rx_pbuf->data = data;
		rx_pbuf->id = i;
		entry.size = IDA_LWIP_MEM_TO_CLASSIC_ENTRY_SIZE;
		entry.ref = (u32_t)i;
		/* put in txFree Queue */
		if(ida_lwip_virtEth_queuePut(&_ida_lwip_sharedMem->freeRxBuffers,&entry) == 0){
			/* if it is successfull, go on with next descriptor */
			return ERR_MEM;
		}
	}

	XIpiPsu_CfgInitialize(&IPIInstance, &XIpiPsu_ConfigTable[0], XIpiPsu_ConfigTable[0].BaseAddress);
	XIpiPsu_InterruptEnable(&IPIInstance, XIPIPSU_ALL_MASK);
	XIpiPsu_ClearInterruptStatus(&IPIInstance, XIPIPSU_ALL_MASK);
	UCOS_IntVectSet(XIpiPsu_ConfigTable[0].IntId, 0, 0, ida_lwip_virtEth_irq_handler, DEF_NULL);
	UCOS_IntSrcEn(XIpiPsu_ConfigTable[0].IntId);

	/* maximum transfer unit */
#ifdef ZYNQMP_USE_JUMBO
	netif->mtu = XEMACPS_MTU_JUMBO - XEMACPS_HDR_SIZE;
#else
	netif->mtu = 1486;//XEMACPS_MTU - XEMACPS_HDR_SIZE;
#endif

#if LWIP_IGMP || defined(IDA_LWIP)
//	netif->igmp_mac_filter = (netif_igmp_mac_filter_fn) xemacpsif_mac_filter_update;
#endif

#if LWIP_IPV6 && LWIP_IPV6_MLD
 netif->mld_mac_filter = xemacpsif_mld6_mac_filter_update;
#endif

	netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

#if LWIP_IPV6 && LWIP_IPV6_MLD
	netif->flags |= NETIF_FLAG_MLD6;
#endif

#if LWIP_IGMP || defined(IDA_LWIP)
	netif->flags |= NETIF_FLAG_IGMP;
#endif

//#if LWIP_FULL_CSUM_OFFLOAD_RX && LWIP_FULL_CSUM_OFFLOAD_TX
//	XEmacPs_EnableChecksumOffload(&xemacpsif->emacps);
//#endif

	return ERR_OK;
}

//#if LWIP_IGMP
//static void xemacpsif_mac_hash_update (struct netif *netif, u8_t *ip_addr,
//		u8_t action)
//{
//	u8_t multicast_mac_addr[6];
//	struct xemac_s *xemac = (struct xemac_s *) (netif->state);
//	xemacpsif_s *xemacpsif = (xemacpsif_s *) (xemac->state);
//	XEmacPs_BdRing *txring;
//	txring = &(XEmacPs_GetTxRing(&xemacpsif->emacps));
//
//	multicast_mac_addr[0] = 0x01;
//	multicast_mac_addr[1] = 0x00;
//	multicast_mac_addr[2] = 0x5E;
//	multicast_mac_addr[3] = ip_addr[1] & 0x7F;
//	multicast_mac_addr[4] = ip_addr[2];
//	multicast_mac_addr[5] = ip_addr[3];
//
//	/* Wait till all sent packets are acknowledged from HW */
//	while(txring->HwCnt);
//
//	SYS_ARCH_DECL_PROTECT(lev);
//
//	SYS_ARCH_PROTECT(lev);
//
//	/* Stop Ethernet */
//	XEmacPs_Stop(&xemacpsif->emacps);
//
//	if (action == IGMP_ADD_MAC_FILTER) {
//		/* Set Mulitcast mac address in hash table */
//		XEmacPs_SetHash(&xemacpsif->emacps, multicast_mac_addr);
//
//	} else if (action == IGMP_DEL_MAC_FILTER) {
//		/* Remove Mulitcast mac address in hash table */
//		XEmacPs_DeleteHash(&xemacpsif->emacps, multicast_mac_addr);
//	}
//
//	/* Reset DMA */
//	reset_dma(xemac);
//
//	/* Start Ethernet */
//	XEmacPs_Start(&xemacpsif->emacps);
//
//	SYS_ARCH_UNPROTECT(lev);
//}
//
//static err_t xemacpsif_mac_filter_update (struct netif *netif, ip_addr_t *group,
//		u8_t action)
//{
//	u8_t temp_mask;
//	unsigned int i;
//	u8_t * ip_addr = (u8_t *) group;
//
//	if ((ip_addr[0] < 224) && (ip_addr[0] > 239)) {
//		LWIP_DEBUGF(NETIF_DEBUG,
//				("%s: The requested MAC address is not a multicast address.\r\n", __func__));
//		LWIP_DEBUGF(NETIF_DEBUG,
//				("Multicast address add operation failure !!\r\n"));
//
//		return ERR_ARG;
//	}
//
//	if (action == IGMP_ADD_MAC_FILTER) {
//
//		for (i = 0; i < XEMACPS_MAX_MAC_ADDR; i++) {
//			temp_mask = (0x01) << i;
//			if ((xemacps_mcast_entry_mask & temp_mask) == temp_mask) {
//				continue;
//			}
//			xemacps_mcast_entry_mask |= temp_mask;
//
//			/* Update mac address in hash table */
//			xemacpsif_mac_hash_update(netif, ip_addr, action);
//
//			LWIP_DEBUGF(NETIF_DEBUG,
//					("%s: Muticast MAC address successfully added.\r\n", __func__));
//
//			return ERR_OK;
//		}
//		if (i == XEMACPS_MAX_MAC_ADDR) {
//			LWIP_DEBUGF(NETIF_DEBUG,
//					("%s: No multicast address registers left.\r\n", __func__));
//			LWIP_DEBUGF(NETIF_DEBUG,
//					("Multicast MAC address add operation failure !!\r\n"));
//
//			return ERR_MEM;
//		}
//	} else if (action == IGMP_DEL_MAC_FILTER) {
//		for (i = 0; i < XEMACPS_MAX_MAC_ADDR; i++) {
//			temp_mask = (0x01) << i;
//			if ((xemacps_mcast_entry_mask & temp_mask) != temp_mask) {
//				continue;
//			}
//			xemacps_mcast_entry_mask &= (~temp_mask);
//
//			/* Update mac address in hash table */
//			xemacpsif_mac_hash_update(netif, ip_addr, action);
//
//			LWIP_DEBUGF(NETIF_DEBUG,
//					("%s: Multicast MAC address successfully removed.\r\n", __func__));
//
//			return ERR_OK;
//		}
//		if (i == XEMACPS_MAX_MAC_ADDR) {
//			LWIP_DEBUGF(NETIF_DEBUG,
//					("%s: No multicast address registers present with\r\n", __func__));
//			LWIP_DEBUGF(NETIF_DEBUG,
//					("the requested Multicast MAC address.\r\n"));
//			LWIP_DEBUGF(NETIF_DEBUG,
//					("Multicast MAC address removal failure!!.\r\n"));
//
//			return ERR_MEM;
//		}
//	}
//	return ERR_OK;
//}
//#endif

/*
 * xemacpsif_init():
 *
 * Should be called at the beginning of the program to set up the
 * network interface. It calls the function low_level_init() to do the
 * actual setup of the hardware.
 *
 */

err_t ida_lwip_virtEth_init(struct netif *netif)
{
	netif->name[0] = IFNAME0;
	netif->name[1] = IFNAME1;
	netif->output = etharp_output;
	netif->linkoutput = low_level_output;

	sys_sem_new(&rxSemaphore, 0);

#if LWIP_IPV6
	netif->output_ip6 = ethip6_output;
#endif

	low_level_init(netif);
	return ERR_OK;
}
