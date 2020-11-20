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

#define ETHARP_TRUST_IP_MAC     0
#define IP_REASSEMBLY           0
#define IP_FRAG                 0
#define ARP_QUEUEING            0

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
#define MEM_ALIGNMENT           8

/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
#define MEM_SIZE                (18*1024)

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#define MEMP_NUM_PBUF           30
/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB        22
/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#define MEMP_NUM_NETBUF			22
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
#define LWIP_TCP                0



/* ---------- ICMP options ---------- */
#define LWIP_ICMP                       0

/* ---------- DHCP options ---------- */
/* Define LWIP_DHCP to 1 if you want DHCP configuration of
   interfaces. DHCP is not implemented in lwIP 0.5.1, however, so
   turning this on does currently not work. */
#define LWIP_DHCP               0

#define LWIP_NETIF_HOSTNAME      1

/* ---------- DNS options ---------- */
#define LWIP_DNS					0

/* ---------- UDP options ---------- */
#define LWIP_UDP                1
#define UDP_TTL                 255

#define	LWIP_IGMP			0

/* ---------- Statistics options ---------- */
#undef LWIP_PROVIDE_ERRNO
/**
 * LWIP_NETCONN==1: Enable Netconn API (require to use api_lib.c)
 */
#define LWIP_NETCONN                    1
/**
 * LWIP_SOCKET==1: Enable Socket API (require to use sockets.c)
 */
#define LWIP_SOCKET                     1


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

#define IDA_LWIP_SOCK_SUPERV_TASK_STACK_SIZE	128 		// 256 before optimization
#define IDA_LWIP_CLASSIC_ADAPTER_STACK_SIZE		128 	// 256 before optimization
#define IDA_LWIP_TX_FILTER_STACK_SIZE			256		// 256 before optimization
#define IDA_LWIP_RX_FILTER_STACK_SIZE			128		// 256 before optimization

#define LWIP_SYS_ARCH_MBOX_SIZE				50
#define DEFAULT_UDP_RECVMBOX_SIZE       LWIP_SYS_ARCH_MBOX_SIZE
#define DEFAULT_TCP_RECVMBOX_SIZE       LWIP_SYS_ARCH_MBOX_SIZE
#define DEFAULT_ACCEPTMBOX_SIZE         LWIP_SYS_ARCH_MBOX_SIZE



#define MEMP_OVERFLOW_CHECK 0
#define LWIP_TIMEVAL_PRIVATE 0

#define DEFAULT_THREAD_STACKSIZE		256
#define DEFAULT_THREAD_PRIO				25

#define LWIP_PTP	1

#define LWIP_TIMERS 0

#define LWIP_NETIF_LOOPBACK	0
#define LWIP_PTP	1
#define PTP_NO_DELAY_REQ 1

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

#define IDA_LWIP_MAX_MSS 1500
#define IDA_LWIP_QUEUE_SIZE		32
#define IDA_LWIP_CLASSIC_QUEUE_TRIGGER	32

#define LWIP_SYS_ARCH_NUMBER_OF_MBOXES			3
#define LWIP_SYS_ARCH_NUMBER_OF_SEMAPHORES		32

#define LWIP_SINGLE_NETIF 	1

#define XLWIP_CONFIG_N_RX_DESC 16
#define XLWIP_CONFIG_N_TX_DESC 16


#define LWIP_DECLARE_MEMORY_ALIGNED(variable_name, size) u8_t variable_name[LWIP_MEM_ALIGN_BUFFER(size)] __attribute__ ((aligned (4))) __attribute__ ((section (".psu_lwip_tcm")));

#define LWIP_MEMORY_ALIASING
#define LWIP_ALIASED_OFFSET		0xFFE00000
#define LWIP_ALIASED_BASE		0x00000000
#define LWIP_ALIASED_SIZE		0x00040000

#define IDA_LWIP_MEMORY_REGION_BASE		0x20000
#define IDA_LWIP_MEMORY_REGION_SIZE		0x20000

#define OS_IS_UCOS

#define LWIP_TCPIP_CORE_LOCKING 	0

#define IDA_LWIP_PARTIAL_UDP_RX	1

//#define SNTP_SET_SYSTEM_TIME_US(sec, us)	updateTime(sec, us)

#endif /* __LWIP_OPT_H__ */


