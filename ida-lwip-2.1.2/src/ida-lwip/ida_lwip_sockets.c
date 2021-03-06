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

#include "lwip/sockets.h"
#include "lwip/priv/sockets_priv.h"
#include "lwip/api.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
//#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/mld6.h"

#include "ida-lwip/ida_lwip_queue.h"
#include "ida-lwip/ida_lwip_filter.h"


#ifndef IDA_LWIP_SOCK_SUPERV_TASK_STACK_SIZE
#define IDA_LWIP_SOCK_SUPERV_TASK_STACK_SIZE 256
#endif
#define SOCK_SUPERV_TASK_PRIO OS_LOWEST_PRIO - 11 //same as dummy task
static CPU_STK sockSupervTaskStk[IDA_LWIP_SOCK_SUPERV_TASK_STACK_SIZE];

#if LWIP_CHECKSUM_ON_COPY
#include "lwip/inet_chksum.h"
#endif

#if LWIP_COMPAT_SOCKETS == 2 && LWIP_POSIX_SOCKETS_IO_NAMES
#include <stdarg.h>
#endif

#include <string.h>

#ifdef LWIP_HOOK_FILENAME
#include LWIP_HOOK_FILENAME
#endif

#if LWIP_IPV4
#define IP4ADDR_PORT_TO_SOCKADDR(sin, ipaddr, port) do { \
      (sin)->sin_len = sizeof(struct sockaddr_in); \
      (sin)->sin_family = AF_INET; \
      (sin)->sin_port = lwip_htons((port)); \
      inet_addr_from_ip4addr(&(sin)->sin_addr, ipaddr); \
      memset((sin)->sin_zero, 0, SIN_ZERO_LEN); }while(0)
#define SOCKADDR4_TO_IP4ADDR_PORT(sin, ipaddr, port) do { \
    inet_addr_to_ip4addr(ip_2_ip4(ipaddr), &((sin)->sin_addr)); \
    (port) = lwip_ntohs((sin)->sin_port); }while(0)
#endif /* LWIP_IPV4 */

#define IS_SOCK_ADDR_LEN_VALID(namelen)  ((namelen) == sizeof(struct sockaddr_in))
#define IS_SOCK_ADDR_TYPE_VALID(name)    ((name)->sa_family == AF_INET)
#define IPADDR_PORT_TO_SOCKADDR(sockaddr, ipaddr, port) \
        IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in*)(void*)(sockaddr), ip_2_ip4(ipaddr), port)
#define SOCKADDR_TO_IPADDR_PORT(sockaddr, ipaddr, port) \
        SOCKADDR4_TO_IP4ADDR_PORT((const struct sockaddr_in*)(const void*)(sockaddr), ipaddr, port)


#define IS_SOCK_ADDR_ALIGNED(name)      ((((mem_ptr_t)(name)) % 4) == 0)


/** A struct sockaddr replacement that has the same alignment as sockaddr_in/
 *  sockaddr_in6 if instantiated.
 */
union sockaddr_aligned {
  struct sockaddr sa;
#if LWIP_IPV6
  struct sockaddr_in6 sin6;
#endif /* LWIP_IPV6 */
#if LWIP_IPV4
  struct sockaddr_in sin;
#endif /* LWIP_IPV4 */
};

/* Define the number of IPv4 multicast memberships, default is one per socket */
#ifndef LWIP_SOCKET_MAX_MEMBERSHIPS
#define LWIP_SOCKET_MAX_MEMBERSHIPS NUM_SOCKETS
#endif


#if LWIP_IPV4 && LWIP_IPV6
static void
sockaddr_to_ipaddr_port(const struct sockaddr *sockaddr, ip_addr_t *ipaddr, u16_t *port)
{
  if ((sockaddr->sa_family) == AF_INET6) {
    SOCKADDR6_TO_IP6ADDR_PORT((const struct sockaddr_in6 *)(const void *)(sockaddr), ipaddr, *port);
    ipaddr->type = IPADDR_TYPE_V6;
  } else {
    SOCKADDR4_TO_IP4ADDR_PORT((const struct sockaddr_in *)(const void *)(sockaddr), ipaddr, *port);
    ipaddr->type = IPADDR_TYPE_V4;
  }
}
#endif /* LWIP_IPV4 && LWIP_IPV6 */

#define IPTYPE IPADDR_TYPE_V4

typedef enum {
	SOCKET_MGM_CREATE = 0,
	SOCKET_MGM_CREATE_PROXY,
	SOCKET_MGM_REGISTER_PROXY,
	SOCKET_MGM_BIND,
	SOCKET_MGM_CLOSE,
} IDA_LWIP_SOCKET_MGM_TYPE;

typedef struct {
	IDA_LWIP_SOCKET_MGM_TYPE type;
	int socket;
	void *data;
	u32_t dataLen;
	int returnValue;
	sys_sem_t ackSem;
} IDA_LWIP_SOCKET_MGM;


LWIP_MEMPOOL_DECLARE(SOCKET_MGM_POOL, 20, sizeof(IDA_LWIP_SOCKET_MGM), "Socket Management Message Pool");

/** The global array of available sockets */
static struct ida_lwip_sock sockets[NUM_SOCKETS];
static struct ida_lwip_proxy_sock proxySockets[NUM_PROXY_SOCKETS];
static struct ida_lwip_sock *socketFreeList;
static struct ida_lwip_proxy_sock *proxySocketFreeList;
static u8_t socketPriorityMap[(NUM_SOCKETS/2)+1];


/*functions*/
struct ida_lwip_proxy_sock *get_proxySocket(int fd);
static err_t lwip_recvfrom_udp_raw(struct lwip_sock *sock, int flags, struct msghdr *msg, u16_t *datagram_len, int dbg_s);
static void free_socket(struct lwip_sock *sock, int is_tcp);
static int _ida_lwip_free_socket(int socket);
void ida_lwip_init(void);

//static sys_mbox_t socket_managment_queue:
static sys_mbox_t socket_mgm_queue;


#define _IDA_LWIP_IS_SOCKET(fd) (fd < NUM_SOCKETS ? 1 : 0)
#define _IDA_LWIP_IS_PROXY(fd)  (fd >= NUM_SOCKETS && fd < NUM_SOCKETS + NUM_PROXY_SOCKETS ? 1 : 0)

#define MONITOR_TRIGGER	5

__attribute__((weak)) void ida_lwip_socket_sendto_hook(void *data, size_t size) { (void) data; return; }

/**
 * INTERNAL_SOCKET_HANDLING
 * Internal socket handling by supervisor task(*((*(socket_mgm_queue)).osmbox)).OSEventType
 */

/*
 * Function to initialize list of sockets and proxy sockets and start of supervisor task
 *
 * */
void ida_lwip_initSockets(void){
	CPU_SR cpu_sr;

	OS_ENTER_CRITICAL();
	LWIP_MEMPOOL_INIT(SOCKET_MGM_POOL);
	sys_mbox_new(&socket_mgm_queue,20);

	/* Initialize all normal sockets, make coherent list */
	for(int i = 0; i < NUM_SOCKETS; i++){
		sockets[i].id = i;
		sockets[i].queue = NULL;
		sockets[i].rxSem = NULL;
		sockets[i].proxy = NULL;
		sockets[i].pendingCounter = 0;

		if(i < NUM_SOCKETS - 1)
			sockets[i].next = &sockets[i+1];
		else
			sockets[i].next = NULL;

		ida_lwip_set_socket_prio(i,0);
	}

	/* Initialize all proxy sockets, make coherent list for proxy sockets */
	for(int i = 0; i < NUM_PROXY_SOCKETS; i++){
		proxySockets[i].id = i + NUM_SOCKETS;
		proxySockets[i].prioQueue = (IDA_LWIP_PRIO_QUEUE*)NULL;
		if(i < NUM_PROXY_SOCKETS - 1)
			proxySockets[i].next = &proxySockets[i+1];
		else
			proxySockets[i].next = NULL;
	}

	/* Save first element of normal socket list and proxy socket list as first free elements */
	socketFreeList = &sockets[0];
	proxySocketFreeList = &proxySockets[0];

	OS_EXIT_CRITICAL();

	/* Start socket supervisor task */
	sys_thread_new("ida_lwip_sockSupervisor", (void (*)(void*)) ida_lwip_socketSupervisorTask, NULL, IDA_LWIP_SOCK_SUPERV_TASK_STACK_SIZE, OS_LOWEST_PRIO - 11);
}

/*
 * Function to enqueue packet into socket's queue (called by supervisor task)
 *
 * @param void* arg: ida_lwip_socket s
 * @param pcb: pointer to pcb
 * @param p: pointer packet buffer
 * @param addr: currently unused
 * @param port: currently unused
 * */
static void _ida_lwip_socketRecv(void *arg, struct udp_pcb *pcb, struct pbuf *p,const ip_addr_t *addr, u16_t port){
	CPU_SR cpu_sr;
	struct ida_lwip_sock *s;

	u16_t len;

	/* Check whether pcb and ida_lwip_socket are valid */
	if(pcb == NULL || arg == NULL){
		pbuf_free(p);
		return;
	}

	s = (struct ida_lwip_sock *)arg;

	if(s == NULL){
		pbuf_free(p);
		return;
	}

	/* Check whether socket's message box is valid */
	if (s->queue == (IDA_LWIP_QUEUE*)0) {
		pbuf_free(p);
		return;
	}

	/* Reset p->copied_len */
	p->copied_len = 0;

	/* Post received pbuf in message box of socket, return if posting was not possible */
	OS_ENTER_CRITICAL();
	if(ida_lwip_queue_put(s->queue,p) == -1){
		pbuf_free(p);
		OS_EXIT_CRITICAL();
		return;
	}

	if(s->proxy != NULL){
		/* Receive data for proxy socket */
		u8_t prio = ida_lwip_get_socket_prio(s->id);
		/* Put the socket id as "payload" in the priority queue of the proxy socket
		 * suppress warning with typecast to u32_t */
		ida_lwip_prioQueuePut(s->proxy->prioQueue,(void*)(u32_t)s->id,prio);
	} else {
		/* Receive data for normal socket */
		sys_sem_signal(&s->rxSem);
	}
	s->pendingCounter++;

	OS_EXIT_CRITICAL();
}

/*
 * Function create a ida lwip socket
 *
 * @return ida lwip socket
 * */
static int _ida_lwip_socketCreate(){
	struct ida_lwip_sock *s;
	sys_sem_t rxSem;
	sys_sem_t txSem;
	IDA_LWIP_QUEUE *queue;
	CPU_SR cpu_sr;
	u16_t monitorTrigger = MONITOR_TRIGGER; /* TODO: variable rauswerfen und define direkt benutzen */

	/* Create new message box for the socket */
	queue = ida_lwip_queue_alloc(IDA_LWIP_QUEUE_SIZE);
	if(queue == (IDA_LWIP_QUEUE*)0){
		return -1;
	}

	/* Create new semaphores for the socket, free created mbox and sem in case of fail */
	if(sys_sem_new(&rxSem,0) != ERR_OK){
		ida_lwip_queue_free(queue);
		return -1;
	}

	if(sys_sem_new(&txSem,0) != ERR_OK){
		ida_lwip_queue_free(queue);
		sys_sem_free(&rxSem);
		return -1;
	}

	OS_ENTER_CRITICAL();

	/* Get first free element of socket list */
	s = socketFreeList;

	/* Check if free socket is available */
	if(s == NULL){
		OS_EXIT_CRITICAL();
		sys_sem_free(&txSem);
		sys_sem_free(&rxSem);
		ida_lwip_queue_free(queue);
		return -1;
	}

	/* Detach it from the free list */
	socketFreeList = s->next;
	s->next = NULL;
	s->queue = queue;
	s->rxSem = rxSem;
	s->txSem = txSem;
	s->proxy = NULL;
	s->pendingCounter = 0;

	OS_EXIT_CRITICAL();

	return s->id;
}

/*
 * Function to create ida lwip proxy socket
 *
 * @return ida lwip proxy socket
 * */
static int _ida_lwip_proxySocketCreate(){
	struct ida_lwip_proxy_sock *s;
	IDA_LWIP_PRIO_QUEUE *prioQueue;
	CPU_SR cpu_sr;

	/* Create the priority queue for proxy socket */
	prioQueue = ida_lwip_prioQueueCreate(IDA_LWIP_QUEUE_SIZE);

	if(prioQueue == NULL){
		return -1;
	}

	OS_ENTER_CRITICAL();

	/* Get first free element of socket list */
	s = proxySocketFreeList;

	/* Check if free socket is available */
	if(s == NULL){
		OS_EXIT_CRITICAL();
		ida_lwip_prioQueueDestroy(prioQueue);
		return -1;
	}

	/* Detach it from the free list */
	proxySocketFreeList = s->next;
	s->next = NULL;

	s->prioQueue = prioQueue;

	OS_EXIT_CRITICAL();

	return s->id;
}

/*
 * Function to bind socket to sockaddr
 *
 * @param s: id of socket
 * @param name: socket address
 * @param namelen: size of sockaddr
 * */
static int _ida_lwip_socketBind(int s, const struct sockaddr *name, socklen_t namelen){
	struct ida_lwip_sock *sock;
	struct sockaddr_in *name_in = (struct sockaddr_in *)&name;
	ip_addr_t local_addr;
	u16_t local_port;
	struct udp_pcb *pcb, *pcb_i;

	/* Set local ipaddr */
	ip_addr_set_ipaddr(&local_addr, (ip_addr_t*)&name_in->sin_addr);

	sock = get_socket(s);
	if (sock == NULL){
		return -1;
	}
	//todo check whether addr valid necessary?
	/* Check size, family and alignment of 'name' */
	if(!(IS_SOCK_ADDR_LEN_VALID(namelen) && IS_SOCK_ADDR_TYPE_VALID(name) && IS_SOCK_ADDR_ALIGNED(name))){
		return -1;
	}

	/* Set local port */
	SOCKADDR_TO_IPADDR_PORT(name, &local_addr, local_port);

	/* Create DDP PCB */
	pcb = udp_new_ip_type(IPTYPE);

	if(pcb == NULL){
		return -1;
	}

	/* Set function to be called, when packets are received via this PCB*/
	pcb->recv = (udp_recv_fn)_ida_lwip_socketRecv;
	pcb->recv_arg = sock;

	/* Link PCB to socket */
	sock->pcb = pcb;

	/* Bind the UDP PCB */
	if(udp_bind(pcb, &local_addr, local_port) != ERR_OK){
		return -1;
	}

	return 0;
//	if(local_port == 0){
//
//		return -1;
//
//	} else {
//	    for (pcb_i = udp_pcbs; pcb_i != NULL; pcb_i = pcb_i->next) {
//	      if (pcb_i != pcb) {
//	          /* By default, we don't allow to bind to a port that any other udp
//	           PCB is already bound to, unless *all* PCBs with that port have the
//	           REUSEADDR flag set. */
//	          /* port matches that of PCB in list and REUSEADDR not set -> reject */
//	          if ((pcb_i->local_port == local_port) && (ip_addr_cmp(&pcb_i->local_ip, &local_addr) || ip_addr_isany(&local_addr) ||  ip_addr_isany(&pcb_i->local_ip))) {
//	              /* other PCB already binds to this local IP and port */
//	              //LWIP_DEBUGF(UDP_DEBUG, ("udp_bind: local port %"U16_F" already bound by another pcb\n", port));
//
//	            return -1;
//	         }
//	       }
//	     }
//	  }
//	//todo: check whether port already bound to other pcbs necessary?
//
//	ip_addr_set_ipaddr(&pcb->local_ip, &local_addr);
//	pcb->local_port = local_port;
//
//    /* place the PCB on the active list if not already there */
//    pcb->next = udp_pcbs;
//    udp_pcbs = pcb;

//	return 0;

}

/*
 * Function to register proxy socket
 *
 * @param poxyFd: proxy socket id
 * @param socketFd: socket id
 * */
int _ida_lwip_registerProxySocket(int proxyFd, int socketFd){
	struct ida_lwip_proxy_sock *proxy;
	struct ida_lwip_sock *socket;
	u16_t pendingPackets = 0;

	/* Check if socket id's are valid */
	if(!_IDA_LWIP_IS_PROXY(proxyFd) || !_IDA_LWIP_IS_SOCKET(socketFd))
		return -1;

	CPU_SR cpu_sr;

	OS_ENTER_CRITICAL();

	/* Get sockets to given socket id's */
	proxy = get_proxySocket(proxyFd);
	socket = get_socket(socketFd);

	/* Exit with error if socket's proxy is already in use */
	if(socket -> proxy != NULL){
		OS_EXIT_CRITICAL();
		return -1;
	}


	socket->proxy = proxy;
	pendingPackets = socket->pendingCounter;

	/* If there were packets received on the socket already,
	 * queue a message into proxy priority queue with socket's priority
	 * todo: zu ungenau? */
	if(pendingPackets > 0){
		u8_t prio = ida_lwip_get_socket_prio(socketFd);
		ida_lwip_prioQueuePut(proxy->prioQueue,(void*)socketFd,prio);
	}

	/* todo: comment */
	sys_sem_free(&socket->rxSem);
	socket->rxSem = NULL;

	OS_EXIT_CRITICAL();
}

/*
 * Function to close a socket
 *
 * @param socket: id of socket to free
 *
 * */
static int _ida_lwip_free_socket(int socket)
{
	if(_IDA_LWIP_IS_SOCKET(socket)){
		/* Close a normal socket */
		struct ida_lwip_sock *sock;

		sock = get_socket(socket);
		if(sock == NULL)
			return -1;
		/* Protect socket array */
		sys_sem_free(&sock->rxSem);
		ida_lwip_queue_free(sock->queue);

		sock->rxSem = NULL;
		sock->queue = NULL;

		/* put socket back into free list*/
		sock->next = socketFreeList;
		socketFreeList = sock;

		return 0;
	} else if(_IDA_LWIP_IS_PROXY(socket)) {
		/* Close a proxy socket */
		struct ida_lwip_proxy_sock * sock = get_proxySocket(socket);
		if(sock == NULL)
			return -1;

		ida_lwip_prioQueueDestroy(sock->prioQueue);
		sock->prioQueue = (IDA_LWIP_PRIO_QUEUE*)NULL;

		sock->next = proxySocketFreeList;
		proxySocketFreeList = sock;

		/* Not implemented yet todo:*/
		return -1;
	} else {
		return -1;
	}
}

/*
 * Task to supervise requests for usage of managment functions todo: is request the right wording?
 *
 * @param p_arg unused
 * */
void ida_lwip_socketSupervisorTask(void *p_arg){
	(void)p_arg;
	IDA_LWIP_SOCKET_MGM *msg;

	while(1){
		sys_arch_mbox_fetch(&socket_mgm_queue,(void*)&msg,0);
		if(msg != NULL){
			switch(msg->type){
			case SOCKET_MGM_CREATE:
				/* Request to create socket -> call socketCreate-function*/
				msg->returnValue = _ida_lwip_socketCreate();
				break;

			case SOCKET_MGM_CREATE_PROXY:
				/* Request to create proxy socket -> call proxySocketCreate-function*/
				msg->returnValue = _ida_lwip_proxySocketCreate();
				break;

			case SOCKET_MGM_BIND:
				/* Request to bind socket -> call socketBind-function */
				msg->returnValue = _ida_lwip_socketBind(msg->socket, msg->data, msg->dataLen);
				break;

			case SOCKET_MGM_CLOSE:
				/* Request to close socket -> call free_socket-function */
				msg->returnValue = _ida_lwip_free_socket(msg->socket);
				break;

			case SOCKET_MGM_REGISTER_PROXY:
				/* Request to register a proxy socket -> call registerProxySocket-function */
				msg->returnValue = _ida_lwip_registerProxySocket(msg->socket, (int)msg->data);
				break;

			default:
				msg->returnValue = -1;

				break;
			}
			/* Signal process that request was processed */
			sys_sem_signal(&(msg->ackSem));
		}
	}
}

/*
 * Function to allocate socket management message
 *
 * @param mgm: pointer to message
 * */
static int _ida_lwip_socketMgmAlloc(IDA_LWIP_SOCKET_MGM **mgm){
	IDA_LWIP_SOCKET_MGM *message = (IDA_LWIP_SOCKET_MGM*)LWIP_MEMPOOL_ALLOC(SOCKET_MGM_POOL);

	if(message == (IDA_LWIP_SOCKET_MGM*)NULL)
		return -1;

	/* Create messages's semaphore */
	sys_sem_new(&message->ackSem,0);
	if(message->ackSem == NULL){
		LWIP_MEMPOOL_FREE(SOCKET_MGM_POOL,message);
		return -1;
	}

	message->returnValue = -1;

	/* Set message */
	*mgm = message;
}

/*
 * Function to free socket management message
 *
 * @param mgm: pointer to message
 * */
static int _ida_lwip_socketMgmFree(IDA_LWIP_SOCKET_MGM *mgm){
	int result = mgm->returnValue;

	/* Free messages's semaphore */
	sys_sem_free(&mgm->ackSem);
	/* Free mesage */
	LWIP_MEMPOOL_FREE(SOCKET_MGM_POOL,mgm);
	return result; // = return mgm->returnValue; <-- todo: is that correct?
}

/*
 * Function to request to register proxy
 * --> sends message with request to register proxy to the supervisor task
 *
 * @param proxy: proxy socket id
 * @param socket: socket id
 */

static int _ida_lwip_registerProxy(int proxy, int socket){
	IDA_LWIP_SOCKET_MGM *msg = NULL;

	if(_ida_lwip_socketMgmAlloc(&msg) < 0)
		return -1;

	/* Set message to request to register proxy*/
	msg->type = SOCKET_MGM_REGISTER_PROXY;
	msg->socket = proxy;
	msg->data = (void*)socket;

	/* Send message to the supervisor task's message queue and
	 * wait for supervisor task to confirm successful processing */
	sys_mbox_post(&socket_mgm_queue,msg);
	sys_arch_sem_wait(&msg->ackSem,0);

	/* return the return value stored in message */
	return _ida_lwip_socketMgmFree(msg);
}

/*
 * Function to get ida lwip socket to given id
 *
 * @param fd: socket id
 * @return ida lwip socket
 * */
struct ida_lwip_sock *get_socket(int fd){
	if(!_IDA_LWIP_IS_SOCKET(fd))
		return NULL;

	struct ida_lwip_sock *sock = &sockets[fd];
	if (sock->queue != NULL){
		return sock;
	}
	return NULL;
}

/*
 * Function to get ida lwip proxy socket to given id
 *
 * @param fd: proxy socket id
 * @return ida lwip proxy socket
 * */
struct ida_lwip_proxy_sock *get_proxySocket(int fd){
	if(!_IDA_LWIP_IS_PROXY(fd))
		return NULL;

	struct ida_lwip_proxy_sock *sock = &proxySockets[fd-NUM_SOCKETS];

	return sock;
}

/****************************************************************************
 * SOCKET HANDLIN API
 * get/set priority of a socket
 ****************************************************************************/

/**
 * Get the priority of a socket
 *
 * @param: int file descriptor of socket
 * @return: u8_t priority [0...7] (3 Bits used)
 */
u8_t ida_lwip_get_socket_prio(int fd){
	if(!_IDA_LWIP_IS_SOCKET(fd))
		return 0;

	int row = fd / 2;
	if(fd % 2 == 0)
		return ((socketPriorityMap[row] & 0x70) >> 4);
	else
		return (socketPriorityMap[row] & 0x07);
}

/**
 * Set the priority of a socket
 *
 * @param: int file descriptor of socket
 * @param: u8_t priority [0...7] (3 Bits used)
 * @return: void
 */
void ida_lwip_set_socket_prio(int fd, u8_t prio){
	CPU_SR cpu_sr;
	if(!_IDA_LWIP_IS_SOCKET(fd))
		return;

	int row = fd / 2;
	OS_ENTER_CRITICAL();
	if(fd % 2 == 0) {
		socketPriorityMap[row] &= ~(0x70);
		socketPriorityMap[row] |= ((prio << 4) & 0x70);
	} else {
		socketPriorityMap[row] &= ~(0x07);
		socketPriorityMap[row] |= (prio & 0x07);
	}
	OS_EXIT_CRITICAL();
}

void ida_lwip_set_socket_buffer(int fd, u16_t trigger){
	CPU_SR cpu_sr;
	if(!_IDA_LWIP_IS_SOCKET(fd))
		return;

	OS_ENTER_CRITICAL();
	ida_lwip_queue_set_trigger(sockets[fd].queue,trigger);
	OS_EXIT_CRITICAL();
}

/****************************************************************************
 * USER_LEVEL_API
 * socket(): Create a new socket
 * bind(): Bind new socket
 * recvFrom(): Read from socket
 * sendTo(): Write to socket
 ****************************************************************************/

/*
 * Function to enqueue management msg to request to create socket
 *
 * @param domain: unused
 * @param type: type of socket
 * @param protocol: unused
 * */
int ida_lwip_socket(int domain, int type, int protocol)
{
	int result;
	IDA_LWIP_SOCKET_MGM *msg = NULL;

	LWIP_UNUSED_ARG(domain);
	LWIP_UNUSED_ARG(protocol);

	/* Check type of socket */
	if(type != SOCK_DGRAM && type != SOCK_PROXY)
	  return -1;

	/* Create message */
	if(_ida_lwip_socketMgmAlloc(&msg) < 0)
		return -1;

	/* Set message to request to create socket */
	if(type == SOCK_DGRAM)
		msg->type = SOCKET_MGM_CREATE;
	else if(type == SOCK_PROXY)
		msg->type = SOCKET_MGM_CREATE_PROXY;
	else {
		_ida_lwip_socketMgmFree(msg);
		return -1;
	}

	/* Send message to the supervisor task's message queue and
	 * wait for supervisor task to confirm successful processing */
	sys_mbox_post(&socket_mgm_queue,msg);
	sys_arch_sem_wait(&msg->ackSem,0);

	/* return the return value stored in message */
	return _ida_lwip_socketMgmFree(msg);
}

/*
 * Function to enqueue management msg to request to bind socket to address
 *
 * @param s: id of socket
 * @param name: socket address
 * @param namelen: size of sockaddr
 * */
int ida_lwip_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
	IDA_LWIP_SOCKET_MGM *msg = NULL;

	/* Create message */
	if(_ida_lwip_socketMgmAlloc(&msg) < 0)
		return -1;

	/* Set message to request bind socket */
	msg->type = SOCKET_MGM_BIND;
	msg->socket = s;
	msg->data = (void*)name;
	msg->dataLen = (u32_t)namelen;

	/* Send message to the supervisor task's message queue and
	 * wait for supervisor task to confirm successful processing */
	sys_mbox_post(&socket_mgm_queue,msg);
	sys_arch_sem_wait(&msg->ackSem,0);

	/* return the return value stored in message */
	return _ida_lwip_socketMgmFree(msg);
}

/*
 * Function to receive packet from socket queue todo: finish comments/ correct comments
 *
 * @param sock: id of socket
 * @param mem: pointer to memory for packet
 * @param len: size
 * @param flags: curently unused
 * @param from: currently unused
 * @param fromlen: currenly unused
 * */
ssize_t ida_lwip_recvfrom(int sock, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	CPU_SR cpu_sr;
	if(_IDA_LWIP_IS_SOCKET(sock)){
		/* Socket is normal socket */
		struct ida_lwip_sock *s;
		struct pbuf* p;
		ssize_t copyLen;

		s = get_socket(sock);
		if (s == NULL)
			return -1;

		/* Wait for arriving data */
		if(s->proxy == NULL)
			sys_arch_sem_wait(&s->rxSem, 0);

		if(s->p_cur == NULL){
			/* Not looked into packet yet */
			OS_ENTER_CRITICAL();
			/* Fetch data from socket's message queue */
			p = ida_lwip_queue_get(s->queue);
			s->pendingCounter--;
			/* Set p_cur */
			s->p_cur = p;
			OS_EXIT_CRITICAL();

			if(p == NULL)
				return -1;
		} else {
			/* already looked into packet
			 * --> set p_cur */
			p = s->p_cur;
		}

		/* Calculate how much has to be copied */
		if(p->tot_len >= len)
			copyLen = len;
		else
			copyLen = p->tot_len;

		/* Copy received data */
		memcpy(mem, (void*)((uint32_t)p->payload+p->copied_len), copyLen);

		p->copied_len += copyLen;

		/* Free pbuf if all data was copied */
		if(p->copied_len == p->tot_len){
			pbuf_free(p);
			s->p_cur = NULL;
		}

		return copyLen;
	} else if(_IDA_LWIP_IS_PROXY(sock)){
		/* Socket is proxy socket */
		struct ida_lwip_proxy_sock* proxy;
		int ready_fd = -1;
		if(len != sizeof(int))
			return -1;

		proxy = get_proxySocket(sock);
		if(proxy == NULL)
			return -1;

		/* Get data from proxy prio queue */
		*(int*)mem = (int)ida_lwip_prioQueuePend(proxy->prioQueue,0);

		return len;
	} else {
		return -1;
	}
}

/*
 * Function to enqueue packet into tx queue
 * --> queue will be processed by filter_tx_thread
 *
 * @param s: id of socket
 * @param data: pointer to data
 * @param size: size of data
 * @param flags: curently unused
 * @param to: sockaddr to send to
 * @param tolen: currenly unused
 * */
ssize_t
ida_lwip_sendto(int s, const void *data, size_t size, int flags,
            const struct sockaddr *to, socklen_t tolen)
{
	ida_lwip_socket_sendto_hook(data, size);
	if(_IDA_LWIP_IS_SOCKET(s)){
		/* Socket is normal socket */
		struct ida_lwip_sock *sock;
		if (size > IDA_LWIP_MAX_MSS) {
		  return -1;
		}
		/* Fill message with data and info */
		IDA_LWIP_TX_REQ txReq;
		txReq.type = UDP;
		txReq.data = data;
		txReq.size = size;
		txReq.to = (void*)to;
		txReq.socket = s;
		txReq.err = ERR_OK;

		sock = get_socket(s);
		if (!sock) {
			return -1;
		}

		txReq.txCompleteSem = sock->txSem;

		u8_t prio = ida_lwip_get_socket_prio(sock->id);
		/* Enqueue message into tx queue */
		ida_filter_enqueue_pkt((void*)&txReq, prio, 0);
		/* Wait for confirmation that packet was handed over to driver */
		sys_arch_sem_wait(&txReq.txCompleteSem, 0);

		return txReq.err == ERR_OK ? size : -1;
	} else if(_IDA_LWIP_IS_PROXY(s)){
		/* Socket is proxy socket */
		if(size != sizeof(int))
			return -1;
		return _ida_lwip_registerProxy(s,*(int*)data); // todo: schöner?
	} else {
	  return -1;
	}
}

/*
 * Function to enqueue packet into tx queue
 * --> queue will be processed by filter_tx_thread
 *
 * @param s: id of socket
 * @param data: pointer to data
 * @param size: size of data
 * @param flags: curently unused
 * @param to: sockaddr to send to
 * @param tolen: currenly unused
 * */
ssize_t
ida_lwip_sendmsg(int s, const struct msghdr *message, int flags)
//(int s, struct iovec *iov, size_t vector_len, int flags,const struct sockaddr *to, socklen_t tolen)
{
	ida_lwip_socket_sendto_hook(message->msg_iov, message->msg_iovlen);
	if(_IDA_LWIP_IS_SOCKET(s)){
		/* Socket is normal socket */
		struct ida_lwip_sock *sock;

		/* Fill message with data and info */
		IDA_LWIP_TX_REQ txReq;
		txReq.type = UDP_ARRAY;
		txReq.data = message->msg_iov;
		txReq.size = message->msg_iovlen;
		txReq.to = message->msg_name;
		txReq.socket = s;
		txReq.err = ERR_OK;

		sock = get_socket(s);
		if (!sock) {
			return -1;
		}

		txReq.txCompleteSem = sock->txSem;

		u8_t prio = ida_lwip_get_socket_prio(sock->id);
		/* Enqueue message into tx queue */
		if(ida_filter_enqueue_pkt((void*)&txReq, prio, 0)!= ERR_OK)
			sys_sem_signal(&txReq.txCompleteSem);
		/* Wait for confirmation that packet was handed over to driver */
		sys_arch_sem_wait(&txReq.txCompleteSem, 0);

		return txReq.err == ERR_OK ? message->msg_iovlen : -1;
	} else {
	  return -1;
	}
}


/*
 * Function to request to close socket
 *
 * @param s: id of socket
 * */
int
ida_lwip_close(int s)
{
	IDA_LWIP_SOCKET_MGM *msg = NULL;

	if(_ida_lwip_socketMgmAlloc(&msg) < 0)
		return -1;

	/* Set message to request closing the socket*/
	msg->type = SOCKET_MGM_CLOSE;
	msg->socket = s;

	/* Send message to the supervisor task's message queue and
	 * wait for supervisor task to confirm successful processing */
	sys_mbox_post(&socket_mgm_queue,msg);
	sys_arch_sem_wait(&msg->ackSem,0);

	/* return the return value stored in message */
	return _ida_lwip_socketMgmFree(msg);
}


