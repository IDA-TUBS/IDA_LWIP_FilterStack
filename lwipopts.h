#ifndef __LWIPOPTS_H__
#define __LWIPOPTS_H__

/**
 * SYS_LIGHTWEIGHT_PROT==1: if you want inter-task protection for certain
 * critical regions during buffer allocation, deallocation and memory
 * allocation and deallocation.
 */
#define SYS_LIGHTWEIGHT_PROT    1

#define ETHARP_TRUST_IP_MAC     0
#define IP_REASSEMBLY           0
#define IP_FRAG                 0
#define ARP_QUEUEING            0


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
#define MEM_ALIGNMENT           4

/* MEM_SIZE: the size of the heap memory. If the application will send
a lot of data that needs to be copied, this should be set high. */
#define MEM_SIZE                (4*1024)

/* MEMP_NUM_PBUF: the number of memp struct pbufs. If the application
   sends a lot of data out of ROM (or other static memory), this
   should be set high. */
#define MEMP_NUM_PBUF           22
/* MEMP_NUM_UDP_PCB: the number of UDP protocol control blocks. One
   per active UDP "connection". */
#define MEMP_NUM_UDP_PCB        22
/* MEMP_NUM_TCP_PCB: the number of simulatenously active TCP
   connections. */
#define MEMP_NUM_TCP_PCB        22
/* MEMP_NUM_TCP_PCB_LISTEN: the number of listening TCP
   connections. */
#define MEMP_NUM_TCP_PCB_LISTEN 22
/* MEMP_NUM_TCP_SEG: the number of simultaneously queued TCP
   segments. */
#define MEMP_NUM_TCP_SEG        22
/* MEMP_NUM_SYS_TIMEOUT: the number of simulateously active
   timeouts. */
#define MEMP_NUM_SYS_TIMEOUT    22
#define MEMP_NUM_NETBUF			22
/**
 * MEMP_NUM_NETCONN: the number of struct netconns.
 * (only needed if you use the sequential API, like api_lib.c)
 */
#define MEMP_NUM_NETCONN         12

///**
// * MEMP_NUM_TCPIP_MSG_API: the number of struct tcpip_msg, which are used
// * for callback/timeout API communication.
// * (only needed if you use tcpip.c)
// */
//#define MEMP_NUM_TCPIP_MSG_API          16
//
///**
// * MEMP_NUM_TCPIP_MSG_INPKT: the number of struct tcpip_msg, which are used
// * for incoming packets.
// * (only needed if you use tcpip.c)
// */
//#define MEMP_NUM_TCPIP_MSG_INPKT        16

/* ---------- Pbuf options ---------- */
/* PBUF_POOL_SIZE: the number of buffers in the pbuf pool. */
#define PBUF_POOL_SIZE          16

/* PBUF_POOL_BUFSIZE: the size of each pbuf in the pbuf pool. */
//#define PBUF_POOL_BUFSIZE       500


/* ---------- TCP options ---------- */
#define LWIP_TCP                1
#define TCP_TTL                 255

/* Controls if TCP should queue segments that arrive out of
   order. Define to 0 if your device is low on memory. */
#define TCP_QUEUE_OOSEQ         1

/* TCP Maximum segment size. */
#define TCP_MSS                 1452      /* TCP_MSS = (Ethernet MTU - IP header size - TCP header size) */

/* TCP sender buffer space (bytes). */
#define TCP_SND_BUF             (8*TCP_MSS)

/*  TCP_SND_QUEUELEN: TCP sender buffer space (pbufs). This must be at least
  as much as (2 * TCP_SND_BUF/TCP_MSS) for things to work. */

#define TCP_SND_QUEUELEN        16

/* TCP receive window. */
#define TCP_WND                 (8*TCP_MSS)


/* ---------- ICMP options ---------- */
#define LWIP_ICMP                       0

/* ---------- IGMP options ---------- */
#define LWIP_IGMP			1

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
/*#define LWIP_DEBUG 1*/
/*#define LWIP_PLATFORM_DIAG(x)    printf x*/
/*#define LWIP_PLATFORM_DIAG(x)    do {printf x;} while(0)*/

#define LWIP_ARP  1
#define LWIP_COMPAT_MUTEX 1

#define LWIP_SO_RCVTIMEO                  1              // default is 0

/*
   ---------------------------------
   ---------- OS options ----------
   ---------------------------------
*/

#define LWIP_COMPAT_MUTEX				1		//Added because we will use semaphores as mutex
#define LWIP_COMPAT_MUTEX_ALLOWED		1		//Allow priority inversion (so far)

//#define LWIP_MAX_THREADS					10
//#define LWIP_LOWEST_PRIO					10
//#define LWIP_MAX_PRIO					LWIP_LOWEST_PRIO + LWIP_MAX_THREADS
//#define SERVER_MAX_SUB_THREADS			3
#define TCPIP_THREAD_PRIO				4
#define TCPIP_MBOX_SIZE					50
#define TCPIP_THREAD_STACKSIZE			512
//#define SERVER_THREAD_STACKSIZE			256

#define DEFAULT_UDP_RECVMBOX_SIZE       50
#define DEFAULT_TCP_RECVMBOX_SIZE       50
#define DEFAULT_ACCEPTMBOX_SIZE         50


#define LWIP_TIMEVAL_PRIVATE 0

#define DEFAULT_THREAD_STACKSIZE		256
#define DEFAULT_THREAD_PRIO				25

#define	ETHARP_SUPPORT_STATIC_ENTRIES   1

//#define SNTP_SET_SYSTEM_TIME_US(sec, us)	updateTime(sec, us)

#endif /* __LWIP_OPT_H__ */


