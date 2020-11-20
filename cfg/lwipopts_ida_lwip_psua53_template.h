#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

//#include "TaskCfg.h"
#include "ucos_ii.h"

/**
 * SYS_LIGHTWEIGHT_PROT==1: if you want inter-task protection for certain
 * critical regions during buffer allocation, deallocation and memory
 * allocation and deallocation.
 */
#define SYS_LIGHTWEIGHT_PROT    1

#define CUSTOM_PBUF				   1
#define LWIP_SUPPORT_CUSTOM_PBUF   1
#define ETHARP_SUPPORT_VLAN		   1
#define LWIP_IPV4                  1
#define LWIP_IPV6                  0

#define ETHARP_TRUST_IP_MAC     1
#define IP_REASSEMBLY           1
#define IP_FRAG                 1
#define ARP_QUEUEING            1

#define ETHARP_SUPPORT_STATIC_ENTRIES 1

#define LWIP_STATS 1

#define LWIP_ERRNO_INCLUDE		<errno.h>

/**
 * NO_SYS==1: Provides VERY minimal functionality. Otherwise,
 * use lwIP facilities.
 */
#define NO_SYS                  0

/* ---------- Memory options ---------- */
/* MEM_ALIGNMENT: should be set to the alignment of the CPU for which
   lwIP is compiled. 4 byte alignment -> define MEM_ALIGNMENT to 4, 2
   byte alignment -> define MEM_ALIGNMENT to 2. */
#define MEM_ALIGNMENT           64

/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
#define MEM_SIZE                (64*1024)

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#define MEMP_NUM_PBUF           5
/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB        22
/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#define MEMP_NUM_TCP_PCB        10
/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#define MEMP_NUM_TCP_PCB_LISTEN 10
/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#define MEMP_NUM_TCP_SEG        64
/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#define MEMP_NUM_SYS_TIMEOUT    10
#define MEMP_NUM_NETBUF			128
/**
 * MEMP_NUM_NETCONN: the number of struct netconns.
 * (only needed if you use the sequential API, like api_lib.c)
 */
#define MEMP_NUM_NETCONN         32

#define MEMP_NUM_TCPIP_MSG_INPKT        32

/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#define PBUF_POOL_SIZE          48
#define PBUF_POOL_BUFSIZE		1522
#define LWIP_PBUF_TIMESTAMP		1


/* ---------- TCP options ---------- */
#define LWIP_TCP                1
#define TCP_TTL                 255

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#define TCP_QUEUE_OOSEQ         1

/* TCP Maximum segment size. */
#define TCP_MSS                 1200      /* TCP_MSS = (Ethernet MTU - IP header size - TCP header size) */

/* TCP receive window. */
#define TCP_WND                 (12*TCP_MSS)

/* TCP sender buffer space (bytes). */
#define TCP_SND_BUF             TCP_WND

/* ---------- ICMP options ---------- */
#define LWIP_ICMP                       1

/* ---------- DHCP options ---------- */
/* Define LWIP_DHCP to 1 if you want DHCP configuration of
   interfaces. DHCP is not implemented in lwIP 0.5.1, however, so
   turning this on does currently not work. */
#define LWIP_DHCP               1

#define LWIP_NETIF_HOSTNAME      1

/* ---------- DNS options ---------- */
#define LWIP_DNS					1		//Enable DNS-Clients

/* ---------- UDP options ---------- */
#define LWIP_UDP                1
#define UDP_TTL                 255

#define	LWIP_IGMP			1

/* ---------- Statistics options ---------- */
//#define LWIP_STATS 0
#undef LWIP_PROVIDE_ERRNO
/**
 * LWIP_NETCONN==1: Enable Netconn API (require to use api_lib.c)
 */
#define LWIP_NETCONN                    1
/**
 * LWIP_SOCKET==1: Enable Socket API (require to use sockets.c)
 */
#define LWIP_SOCKET                     1

#define LWIP_NETIF_TX_SINGLE_PBUF	1


#define LWIP_ARP  1
#define LWIP_COMPAT_MUTEX 1

#define LWIP_SO_RCVTIMEO                  1              // default is 0
#define LWIP_SO_RCVBUF	1

/*
   ---------------------------------
   ---------- OS options ----------
   ---------------------------------
*/

#define LWIP_COMPAT_MUTEX				1		//Added because we will use semaphores as mutex
#define LWIP_COMPAT_MUTEX_ALLOWED		1		//Allow priority inversion (so far)

#define TCPIP_THREAD_PRIO				45
#define TCPIP_MBOX_SIZE					50
#define TCPIP_THREAD_STACKSIZE			1024

#define LWIP_SYS_ARCH_TOTAL_STACK_SIZE  8192

#define LWIP_SYS_ARCH_MBOX_SIZE				100
#define DEFAULT_UDP_RECVMBOX_SIZE       LWIP_SYS_ARCH_MBOX_SIZE
#define DEFAULT_TCP_RECVMBOX_SIZE       LWIP_SYS_ARCH_MBOX_SIZE
#define DEFAULT_ACCEPTMBOX_SIZE         LWIP_SYS_ARCH_MBOX_SIZE



#define MEMP_OVERFLOW_CHECK 0
#define LWIP_TIMEVAL_PRIVATE 0

#define DEFAULT_THREAD_STACKSIZE		256
#define DEFAULT_THREAD_PRIO				25

#define LWIP_PTP	1

#define LWIP_NETIF_LOOPBACK		1	/* Enable loopback */

/*
 * Enable Checksum Generation for UDP, TCP and IP in HW.
 * Enable Checksum Check for UDP, TCP and IP in HW.
 * Packets with faulty cheksum are discarded.
 */
// Checksum Offload
/* ICMP packets are transfered over IP, Checksum are already checked by HW */
#define CHECKSUM_CHECK_ICMP 0
#define CHECKSUM_GEN_ICMP 	1
#define CHECKSUM_CHECK_IP	0
#define CHECKSUM_CHECK_UDP	0
#define CHECKSUM_CHECK_TCP	0
#define CHECKSUM_GEN_UDP    	0
#define CHECKSUM_GEN_IP		0
#define CHECKSUM_GEN_TCP	0



#define LWIP_FULL_CSUM_OFFLOAD_RX  1
#define LWIP_FULL_CSUM_OFFLOAD_TX  1
#define LWIP_CHECKSUM_CTRL_PER_NETIF 0

#define LWIP_SYS_ARCH_NUMBER_OF_MBOXES			40
#define LWIP_SYS_ARCH_NUMBER_OF_SEMAPHORES		32

#define LWIP_SINGLE_NETIF 	1

#define LWIP_TCPIP_CORE_LOCKING 	0

#define OS_IS_UCOS

#define LWIP_TCPIP_CORE_LOCKING 	0
#define	LWIP_ALLOW_MEM_FREE_FROM_OTHER_CONTEXT 1

#define ETHARP_VLAN 10

#define LWIP_HOOK_VLAN_SET(a,b,c,d,e) 		(ETHARP_VLAN)
#define ETHARP_VLAN_CHECK	ETHARP_VLAN

//#define SNTP_SET_SYSTEM_TIME_US(sec, us)	updateTime(sec, us)

#endif /* __LWIP_OPT_H__ */


