/*
 * ida_lwip_sockets.c
 *
 *  Created on: 21.06.2019
 *      Author: kaige
 */

#include "lwip/opt.h"

#include "lwip/sockets.h"
#include "lwip/priv/sockets_priv.h"
#include "lwip/api.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
#include "lwip/tcp.h"
#include "lwip/raw.h"
#include "lwip/udp.h"
#include "lwip/memp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/priv/tcpip_priv.h"
#include "lwip/mld6.h"

#include "ida-lwip/ida_lwip_monitor.h"

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

/* If the netconn API is not required publicly, then we include the necessary
   files here to get the implementation */
#if !LWIP_NETCONN
#undef LWIP_NETCONN
#define LWIP_NETCONN 1
#include "api_msg.c"
#include "api_lib.c"
#include "netbuf.c"
#undef LWIP_NETCONN
#define LWIP_NETCONN 0
#endif

#define API_SELECT_CB_VAR_REF(name)               API_VAR_REF(name)
#define API_SELECT_CB_VAR_DECLARE(name)           API_VAR_DECLARE(struct lwip_select_cb, name)
#define API_SELECT_CB_VAR_ALLOC(name, retblock)   API_VAR_ALLOC_EXT(struct lwip_select_cb, MEMP_SELECT_CB, name, retblock)
#define API_SELECT_CB_VAR_FREE(name)              API_VAR_FREE(MEMP_SELECT_CB, name)

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

#if LWIP_IPV6
#define IP6ADDR_PORT_TO_SOCKADDR(sin6, ipaddr, port) do { \
      (sin6)->sin6_len = sizeof(struct sockaddr_in6); \
      (sin6)->sin6_family = AF_INET6; \
      (sin6)->sin6_port = lwip_htons((port)); \
      (sin6)->sin6_flowinfo = 0; \
      inet6_addr_from_ip6addr(&(sin6)->sin6_addr, ipaddr); \
      (sin6)->sin6_scope_id = ip6_addr_zone(ipaddr); }while(0)
#define SOCKADDR6_TO_IP6ADDR_PORT(sin6, ipaddr, port) do { \
    inet6_addr_to_ip6addr(ip_2_ip6(ipaddr), &((sin6)->sin6_addr)); \
    if (ip6_addr_has_scope(ip_2_ip6(ipaddr), IP6_UNKNOWN)) { \
      ip6_addr_set_zone(ip_2_ip6(ipaddr), (u8_t)((sin6)->sin6_scope_id)); \
    } \
    (port) = lwip_ntohs((sin6)->sin6_port); }while(0)
#endif /* LWIP_IPV6 */

#if LWIP_IPV4 && LWIP_IPV6
static void sockaddr_to_ipaddr_port(const struct sockaddr *sockaddr, ip_addr_t *ipaddr, u16_t *port);

#define IS_SOCK_ADDR_LEN_VALID(namelen)  (((namelen) == sizeof(struct sockaddr_in)) || \
                                         ((namelen) == sizeof(struct sockaddr_in6)))
#define IS_SOCK_ADDR_TYPE_VALID(name)    (((name)->sa_family == AF_INET) || \
                                         ((name)->sa_family == AF_INET6))
#define SOCK_ADDR_TYPE_MATCH(name, sock) \
       ((((name)->sa_family == AF_INET) && !(NETCONNTYPE_ISIPV6((sock)->conn->type))) || \
       (((name)->sa_family == AF_INET6) && (NETCONNTYPE_ISIPV6((sock)->conn->type))))
#define IPADDR_PORT_TO_SOCKADDR(sockaddr, ipaddr, port) do { \
    if (IP_IS_ANY_TYPE_VAL(*ipaddr) || IP_IS_V6_VAL(*ipaddr)) { \
      IP6ADDR_PORT_TO_SOCKADDR((struct sockaddr_in6*)(void*)(sockaddr), ip_2_ip6(ipaddr), port); \
    } else { \
      IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in*)(void*)(sockaddr), ip_2_ip4(ipaddr), port); \
    } } while(0)
#define SOCKADDR_TO_IPADDR_PORT(sockaddr, ipaddr, port) sockaddr_to_ipaddr_port(sockaddr, ipaddr, &(port))
#define DOMAIN_TO_NETCONN_TYPE(domain, type) (((domain) == AF_INET) ? \
  (type) : (enum netconn_type)((type) | NETCONN_TYPE_IPV6))
#elif LWIP_IPV6 /* LWIP_IPV4 && LWIP_IPV6 */
#define IS_SOCK_ADDR_LEN_VALID(namelen)  ((namelen) == sizeof(struct sockaddr_in6))
#define IS_SOCK_ADDR_TYPE_VALID(name)    ((name)->sa_family == AF_INET6)
#define SOCK_ADDR_TYPE_MATCH(name, sock) 1
#define IPADDR_PORT_TO_SOCKADDR(sockaddr, ipaddr, port) \
        IP6ADDR_PORT_TO_SOCKADDR((struct sockaddr_in6*)(void*)(sockaddr), ip_2_ip6(ipaddr), port)
#define SOCKADDR_TO_IPADDR_PORT(sockaddr, ipaddr, port) \
        SOCKADDR6_TO_IP6ADDR_PORT((const struct sockaddr_in6*)(const void*)(sockaddr), ipaddr, port)
#define DOMAIN_TO_NETCONN_TYPE(domain, netconn_type) (netconn_type)
#else /*-> LWIP_IPV4: LWIP_IPV4 && LWIP_IPV6 */
#define IS_SOCK_ADDR_LEN_VALID(namelen)  ((namelen) == sizeof(struct sockaddr_in))
#define IS_SOCK_ADDR_TYPE_VALID(name)    ((name)->sa_family == AF_INET)
#define SOCK_ADDR_TYPE_MATCH(name, sock) 1
#define IPADDR_PORT_TO_SOCKADDR(sockaddr, ipaddr, port) \
        IP4ADDR_PORT_TO_SOCKADDR((struct sockaddr_in*)(void*)(sockaddr), ip_2_ip4(ipaddr), port)
#define SOCKADDR_TO_IPADDR_PORT(sockaddr, ipaddr, port) \
        SOCKADDR4_TO_IP4ADDR_PORT((const struct sockaddr_in*)(const void*)(sockaddr), ipaddr, port)
#define DOMAIN_TO_NETCONN_TYPE(domain, netconn_type) (netconn_type)
#endif /* LWIP_IPV6 */

#define IS_SOCK_ADDR_TYPE_VALID_OR_UNSPEC(name)    (((name)->sa_family == AF_UNSPEC) || \
                                                    IS_SOCK_ADDR_TYPE_VALID(name))
#define SOCK_ADDR_TYPE_MATCH_OR_UNSPEC(name, sock) (((name)->sa_family == AF_UNSPEC) || \
                                                    SOCK_ADDR_TYPE_MATCH(name, sock))
#define IS_SOCK_ADDR_ALIGNED(name)      ((((mem_ptr_t)(name)) % 4) == 0)


#define LWIP_SOCKOPT_CHECK_OPTLEN(sock, optlen, opttype) do { if ((optlen) < sizeof(opttype)) { done_socket(sock); return EINVAL; }}while(0)
#define LWIP_SOCKOPT_CHECK_OPTLEN_CONN(sock, optlen, opttype) do { \
  LWIP_SOCKOPT_CHECK_OPTLEN(sock, optlen, opttype); \
  if ((sock)->conn == NULL) { done_socket(sock); return EINVAL; } }while(0)
#define LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, opttype) do { \
  LWIP_SOCKOPT_CHECK_OPTLEN(sock, optlen, opttype); \
  if (((sock)->conn == NULL) || ((sock)->conn->pcb.tcp == NULL)) { done_socket(sock); return EINVAL; } }while(0)
#define LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB_TYPE(sock, optlen, opttype, netconntype) do { \
  LWIP_SOCKOPT_CHECK_OPTLEN_CONN_PCB(sock, optlen, opttype); \
  if (NETCONNTYPE_GROUP(netconn_type((sock)->conn)) != netconntype) { done_socket(sock); return ENOPROTOOPT; } }while(0)


#define LWIP_SETGETSOCKOPT_DATA_VAR_REF(name)     API_VAR_REF(name)
#define LWIP_SETGETSOCKOPT_DATA_VAR_DECLARE(name) API_VAR_DECLARE(struct lwip_setgetsockopt_data, name)
#define LWIP_SETGETSOCKOPT_DATA_VAR_FREE(name)    API_VAR_FREE(MEMP_SOCKET_SETGETSOCKOPT_DATA, name)
#if LWIP_MPU_COMPATIBLE
#define LWIP_SETGETSOCKOPT_DATA_VAR_ALLOC(name, sock) do { \
  name = (struct lwip_setgetsockopt_data *)memp_malloc(MEMP_SOCKET_SETGETSOCKOPT_DATA); \
  if (name == NULL) { \
    sock_set_errno(sock, ENOMEM); \
    done_socket(sock); \
    return -1; \
  } }while(0)
#else /* LWIP_MPU_COMPATIBLE */
#define LWIP_SETGETSOCKOPT_DATA_VAR_ALLOC(name, sock)
#endif /* LWIP_MPU_COMPATIBLE */

#if LWIP_SO_SNDRCVTIMEO_NONSTANDARD
#define LWIP_SO_SNDRCVTIMEO_OPTTYPE int
#define LWIP_SO_SNDRCVTIMEO_SET(optval, val) (*(int *)(optval) = (val))
#define LWIP_SO_SNDRCVTIMEO_GET_MS(optval)   ((long)*(const int*)(optval))
#else
#define LWIP_SO_SNDRCVTIMEO_OPTTYPE struct timeval
#define LWIP_SO_SNDRCVTIMEO_SET(optval, val)  do { \
  u32_t loc = (val); \
  ((struct timeval *)(optval))->tv_sec = (long)((loc) / 1000U); \
  ((struct timeval *)(optval))->tv_usec = (long)(((loc) % 1000U) * 1000U); }while(0)
#define LWIP_SO_SNDRCVTIMEO_GET_MS(optval) ((((const struct timeval *)(optval))->tv_sec * 1000) + (((const struct timeval *)(optval))->tv_usec / 1000))
#endif


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

#if LWIP_IGMP
/* This is to keep track of IP_ADD_MEMBERSHIP calls to drop the membership when
   a socket is closed */
struct lwip_socket_multicast_pair {
  /** the socket */
  struct lwip_sock *sock;
  /** the interface address */
  ip4_addr_t if_addr;
  /** the group address */
  ip4_addr_t multi_addr;
};

static struct lwip_socket_multicast_pair socket_ipv4_multicast_memberships[LWIP_SOCKET_MAX_MEMBERSHIPS];

static int  lwip_socket_register_membership(int s, const ip4_addr_t *if_addr, const ip4_addr_t *multi_addr);
static void lwip_socket_unregister_membership(int s, const ip4_addr_t *if_addr, const ip4_addr_t *multi_addr);
static void lwip_socket_drop_registered_memberships(int s);
#endif /* LWIP_IGMP */

#if LWIP_IPV6_MLD
/* This is to keep track of IP_JOIN_GROUP calls to drop the membership when
   a socket is closed */
struct lwip_socket_multicast_mld6_pair {
  /** the socket */
  struct lwip_sock *sock;
  /** the interface index */
  u8_t if_idx;
  /** the group address */
  ip6_addr_t multi_addr;
};

static struct lwip_socket_multicast_mld6_pair socket_ipv6_multicast_memberships[LWIP_SOCKET_MAX_MEMBERSHIPS];

static int  lwip_socket_register_mld6_membership(int s, unsigned int if_idx, const ip6_addr_t *multi_addr);
static void lwip_socket_unregister_mld6_membership(int s, unsigned int if_idx, const ip6_addr_t *multi_addr);
static void lwip_socket_drop_registered_mld6_memberships(int s);
#endif /* LWIP_IPV6_MLD */



#define sock_set_errno(sk, e) do { \
  const int sockerr = (e); \
  set_errno(sockerr); \
} while (0)

/* Forward declaration of some functions */
#if !LWIP_TCPIP_CORE_LOCKING
static void lwip_getsockopt_callback(void *arg);
static void lwip_setsockopt_callback(void *arg);
#endif
static int lwip_getsockopt_impl(int s, int level, int optname, void *optval, socklen_t *optlen);
static int lwip_setsockopt_impl(int s, int level, int optname, const void *optval, socklen_t optlen);
static int free_socket_locked(struct lwip_sock *sock, int is_tcp, struct netconn **conn,
                              union lwip_sock_lastdata *lastdata);
static void free_socket_free_elements(int is_tcp, struct netconn *conn, union lwip_sock_lastdata *lastdata);

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

#define sock_inc_used(sock)         1
#define sock_inc_used_locked(sock)  1
#define IPTYPE IPADDR_TYPE_V4

#define SOCK_SUPERV_TASK_STACK_SIZE 1024
#define SOCK_SUPERV_TASK_PRIO 10 //same as dummy task
static CPU_STK sockSupervTaskStk[SOCK_SUPERV_TASK_STACK_SIZE];

typedef enum {
	SOCKET_MGM_CREATE = 0,
	SOCKET_MGM_BIND,
	SOCKET_MGM_SELECT,
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
static struct ida_lwip_sock *socketFreeList;
/** the global array of available mboxes*/
static sys_mbox_t socket_msg_queues[NUM_SOCKETS];
static int socket_msg_queue_counter = 0;
/*functions*/
static struct ida_lwip_sock *get_socket(int fd);
static err_t lwip_recvfrom_udp_raw(struct lwip_sock *sock, int flags, struct msghdr *msg, u16_t *datagram_len, int dbg_s);
static void free_socket(struct lwip_sock *sock, int is_tcp);
static int _ida_free_socket(struct ida_lwip_sock *sock);
void ida_lwip_init(void);

//static sys_mbox_t socket_managment_queue:
static sys_mbox_t socket_mgm_queue;

/**
 * INTERNAL_SOCKET_HANDLING
 * Internal socket handling by supervisor task
 */

static sys_mbox_t* _ida_lwip_get_mbox(){
	sys_mbox_t* mbox;
	if (socket_msg_queue_counter > NUM_SOCKETS){
		return NULL;
	}
	mbox = &socket_msg_queues[socket_msg_queue_counter];
	socket_msg_queue_counter++;
	return mbox;
}

void ida_lwip_initSockets(void){
	LWIP_MEMPOOL_INIT(SOCKET_MGM_POOL);
	sys_mbox_new(&socket_mgm_queue,20);

	for(int i = 0; i < NUM_SOCKETS; i++){
		sockets[i].id = i;
		sockets[i].prio = i;
		sockets[i].mbox = NULL;
		sockets[i].sem = NULL;
		if(i < NUM_SOCKETS - 1)
			sockets[i].next = &sockets[i+1];
		else
			sockets[i].next = NULL;
	}
	socketFreeList = &sockets[0];
//	memset(sockets,0,sizeof(sockets));
}

static void _ida_lwip_socketRecv(void *arg, struct udp_pcb *pcb, struct pbuf *p,const ip_addr_t *addr, u16_t port){

	struct ida_lwip_sock *s;
//	struct netbuf *buf;

	u16_t len;

	if(pcb == NULL || arg == NULL){
		pbuf_free(p);
		return;
	}

	s = (struct ida_lwip_sock *)arg;

	if(s == NULL){
		pbuf_free(p);
		return;
	}

	if (!sys_mbox_valid(s->mbox)) {
		pbuf_free(p);
		return;
	}

	if (!ida_monitor_check(p, s->monitor)){
		pbuf_free(p);
		return;
	}

//	  buf = (struct netbuf *)memp_malloc(MEMP_NETBUF);
//	  if (buf == NULL) {
//	    pbuf_free(p);
//	    return;
//	  } else {
//	    buf->p = p;
//	    buf->ptr = p;
//	    ip_addr_set(&buf->addr, addr);
//	    buf->port = port;
//	  }

	if (sys_mbox_trypost(s->mbox, p) != ERR_OK) {
		pbuf_free(p);
		return;
	}

    /* Register event with callback */
//    API_EVENT(conn, NETCONN_EVT_RCVPLUS, len);
}

static struct udp_pcb* _ida_lwip_newPcb(struct ida_lwip_sock *sock){
	struct udp_pcb* pcb;

	pcb = udp_new_ip_type(IPTYPE);

	if(pcb == NULL){
		return NULL;
	}

	//todo set flags?? udp_setflags(msg->conn->pcb.udp, UDP_FLAGS_NOCHKSUM);

	pcb->recv = (udp_recv_fn)_ida_lwip_socketRecv;
	pcb->recv_arg = sock;

	return pcb;

}


static int _ida_lwip_socketCreate(){
	struct ida_lwip_sock *s;
	sys_mbox_t *mbox = _ida_lwip_get_mbox();
	sys_sem_t *sem;
	PBUF_MONITOR_T *monitor;

	/* TODO: Configure specific priority here */
	u16_t prio = 0;
	u16_t mbox_size = LWIP_SYS_ARCH_MBOX_SIZE;
	u16_t monitorTrigger = 5;

	if(sys_mbox_new(mbox, mbox_size) != ERR_OK){
		return -1;
	}

	if(sys_sem_new(sem,0) != ERR_OK){
		sys_mbox_free(mbox);
		return -1;
	}

	monitor = ida_monitor_alloc(monitorTrigger);
	if(monitor == NULL){
		sys_mbox_free(mbox);
		sys_sem_free(sem);
		return -1;
	}

	s = socketFreeList;

	/* Check if free socket is available */
	if(s == NULL){
		sys_sem_free(sem);
		sys_mbox_free(mbox);
		ida_monitor_free(monitor);
		//todo: here was kai's last edit
		return -1;
	}

	/* detach it from the free list */
	socketFreeList = s->next;
	s->next = NULL;
	if(prio == 0)
		s->prio = s->id;
	s->mbox = mbox;
	s->sem = sem;
	s->monitor = monitor;

	return s->id;
}

static int _ida_lwip_socketBind(int s, const struct sockaddr *name, socklen_t namelen){
	struct ida_lwip_sock *sock;
	ip_addr_t local_addr;
	u16_t local_port;
	struct udp_pcb *pcb;

	sock = get_socket(s);
	if (sock == NULL){
		return -1;
	}
	//todo check whether addr valid necessary?
	/* check size, family and alignment of 'name' */
	if(!(IS_SOCK_ADDR_LEN_VALID(namelen) && IS_SOCK_ADDR_TYPE_VALID(name) && IS_SOCK_ADDR_ALIGNED(name))){
		return -1;
	}

	SOCKADDR_TO_IPADDR_PORT(name, &local_addr, local_port);

	pcb = _ida_lwip_newPcb(sock);
	if(pcb == NULL){
		return -1;
	}

	if(local_port == 0){

		return -1;

	} else {
	    for (pcb = udp_pcbs; pcb != NULL; pcb = pcb->next) {
	      if (pcb != pcb) {
	          /* By default, we don't allow to bind to a port that any other udp
	           PCB is already bound to, unless *all* PCBs with that port have the
	           REUSEADDR flag set. */
	          /* port matches that of PCB in list and REUSEADDR not set -> reject */
	          if ((pcb->local_port == local_port) && (ip_addr_cmp(&pcb->local_ip, &local_addr) || ip_addr_isany(&local_addr) ||  ip_addr_isany(&pcb->local_ip))) {
	              /* other PCB already binds to this local IP and port */
	              //LWIP_DEBUGF(UDP_DEBUG, ("udp_bind: local port %"U16_F" already bound by another pcb\n", port));

	            return -1;
	         }
	       }
	     }
	  }
	//todo: check whether port already bound to other pcbs necessary?

	ip_addr_set_ipaddr(&pcb->local_ip, &local_addr);
	pcb->local_port = local_port;

	return 0;

}

void ida_lwip_socketSupervisorTask(void *p_arg){ // used to be static
	(void)p_arg;
	IDA_LWIP_SOCKET_MGM *msg;

	while(1){
		sys_arch_mbox_fetch(&socket_mgm_queue,(void*)&msg,0);
		if(msg != NULL){
			switch(msg->type){
			case SOCKET_MGM_CREATE:
				msg->returnValue = _ida_lwip_socketCreate();
				sys_sem_signal(&(msg->ackSem));
				break;

			case SOCKET_MGM_BIND:
				msg->returnValue = _ida_lwip_socketBind(msg->socket, msg->data, msg->dataLen);
				sys_sem_signal(&(msg->ackSem));
				break;

			case SOCKET_MGM_SELECT:
//				msg->returnValue = ;
				sys_sem_signal(&(msg->ackSem));
				break;

			case SOCKET_MGM_CLOSE:
				msg->returnValue = _ida_free_socket((struct ida_lwip_sock*)msg->data);
				sys_sem_signal(&(msg->ackSem));
				break;

			default:
				msg->returnValue = -1;
				sys_sem_signal(&(msg->ackSem));
				break;
			}

		}
	}
}

void ida_lwip_init(void){


	ida_lwip_initSockets();

	OSTaskCreateExt(ida_lwip_socketSupervisorTask,
						NULL,
						&sockSupervTaskStk[SOCK_SUPERV_TASK_STACK_SIZE - 1],
						SOCK_SUPERV_TASK_PRIO,
						SOCK_SUPERV_TASK_PRIO,
						&sockSupervTaskStk[0],
						SOCK_SUPERV_TASK_STACK_SIZE,
						DEF_NULL,
						(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));
	OSTaskNameSet(5, (INT8U *) "sockSupervServerTask", NULL);

//	sys_thread_new("sockSupervServerTask", ida_lwip_socketSupervisorTask, NULL, SOCK_SUPERV_TASK_STACK_SIZE, SOCK_SUPERV_TASK_PRIO);

}

static int _ida_lwip_socketMgmAlloc(IDA_LWIP_SOCKET_MGM **mgm){
	IDA_LWIP_SOCKET_MGM *message = (IDA_LWIP_SOCKET_MGM*)LWIP_MEMPOOL_ALLOC(SOCKET_MGM_POOL);

	if(message == (IDA_LWIP_SOCKET_MGM*)NULL)
		return -1;

	sys_sem_new(&message->ackSem,0);
	if(message->ackSem == NULL){
		LWIP_MEMPOOL_FREE(SOCKET_MGM_POOL,message);
		return -1;
	}

	message->returnValue = -1;

	*mgm = message;
}

static int _ida_lwip_socketMgmFree(IDA_LWIP_SOCKET_MGM *mgm){
	int result = mgm->returnValue;
	sys_sem_free(&mgm->ackSem);
	LWIP_MEMPOOL_FREE(SOCKET_MGM_POOL,mgm);
	return result;
}

static struct ida_lwip_sock *get_socket(int fd){
	//todo: Check parameters <-- done?
	struct ida_lwip_sock *sock = &sockets[fd];

	if (sock->mbox != NULL && sock->monitor != NULL && sock->sem != NULL){
		return sock;
	}
	return NULL;
}

/**
 * USER_LEVEL_API
 * socket(): Create a new socket
 * bind(): Bind new socket
 * recvFrom(): Read from socket
 * sendTo(): Write to socket
 */

int ida_lwip_socket(int domain, int type, int protocol)
{
	int result;
	IDA_LWIP_SOCKET_MGM *msg = NULL;

	LWIP_UNUSED_ARG(domain);
	LWIP_UNUSED_ARG(protocol);

	if(type != SOCK_DGRAM)
	  return -1;

	if(_ida_lwip_socketMgmAlloc(&msg) < 0)
		return -1;

	msg->type = SOCKET_MGM_CREATE;

	sys_mbox_post(&socket_mgm_queue,msg);
	sys_arch_sem_wait(&msg->ackSem,0);

	return _ida_lwip_socketMgmFree(msg);
}

int ida_lwip_bind(int s, const struct sockaddr *name, socklen_t namelen)
{
	IDA_LWIP_SOCKET_MGM *msg = NULL;

	if(_ida_lwip_socketMgmAlloc(&msg) < 0)
		return -1;

	msg->type = SOCKET_MGM_BIND;
	msg->socket = s;
	msg->data = (void*)name;
	msg->dataLen = (u32_t)namelen;

	sys_mbox_post(&socket_mgm_queue,msg);
	sys_arch_sem_wait(&msg->ackSem,0);

	return _ida_lwip_socketMgmFree(msg);
}


ssize_t ida_lwip_recvfrom(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	struct ida_lwip_sock *sock;
	struct pbuf* p;
	ssize_t ret;
	u16_t buflen, copylen, copied;
	int i;

	sock = get_socket(s);
	if (sock == NULL) {
		return -1;
	}

	u16_t datagram_len = 0;
	struct iovec vec;
	struct msghdr msg;
	err_t err;
	vec.iov_base = mem;
	vec.iov_len = len;
	msg.msg_control = NULL;
	msg.msg_controllen = 0;
	msg.msg_flags = 0;
	msg.msg_iov = &vec;
	msg.msg_iovlen = 1;
	msg.msg_name = from;
	msg.msg_namelen = (fromlen ? *fromlen : 0);


	//TODO: POST OR FETCH??
	if(sys_arch_mbox_tryfetch(sock->mbox, (void*)p) == SYS_ARCH_TIMEOUT) {
		ida_lwip_close(s);
		return -1;
	}

	if(p == NULL) {
		ida_lwip_close(s);
		return -1;
	}

	buflen = p->tot_len;

	copied = 0;
	/* copy the pbuf payload into the iovs */
	for (i = 0; (i < msg.msg_iovlen) && (copied < buflen); i++) {
		u16_t len_left = (u16_t)(buflen - copied);
		if (msg.msg_iov[i].iov_len > len_left) {
			copylen = len_left;
		} else {
			copylen = (u16_t)msg.msg_iov[i].iov_len;
		}

		/* copy the contents of the received buffer into
			the supplied memory buffer */
		pbuf_copy_partial(p, (u8_t *)msg.msg_iov[i].iov_base, copylen, copied);
		copied = (u16_t)(copied + copylen);
	}

	if (datagram_len) {
		datagram_len = buflen;
	}

	ret = (ssize_t)LWIP_MIN(LWIP_MIN(len, datagram_len), SSIZE_MAX);
	if (fromlen) {
	*fromlen = msg.msg_namelen;
	}

	ida_lwip_close(s);
	return ret;
}

//ssize_t ida_lwip_recvfromOld(int s, void *mem, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
//{
//  struct lwip_sock *sock;
//  ssize_t ret;
//
//  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom(%d, %p, %"SZT_F", 0x%x, ..)\n", s, mem, len, flags));
//  sock = get_socket(s);
//  if (!sock) {
//    return -1;
//  }
//#if LWIP_TCP
//  if (NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP) {
//    ret = lwip_recv_tcp(sock, mem, len, flags);
//    lwip_recv_tcp_from(sock, from, fromlen, "lwip_recvfrom", s, ret);
//    done_socket(sock);
//    return ret;
//  } else
//#endif
//  {
//    u16_t datagram_len = 0;
//    struct iovec vec;
//    struct msghdr msg;
//    err_t err;
//    vec.iov_base = mem;
//    vec.iov_len = len;
//    msg.msg_control = NULL;
//    msg.msg_controllen = 0;
//    msg.msg_flags = 0;
//    msg.msg_iov = &vec;
//    msg.msg_iovlen = 1;
//    msg.msg_name = from;
//    msg.msg_namelen = (fromlen ? *fromlen : 0);
//    err = lwip_recvfrom_udp_raw(sock, flags, &msg, &datagram_len, s);
//    if (err != ERR_OK) {
//      LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom[UDP/RAW](%d): buf == NULL, error is \"%s\"!\n",
//                                  s, lwip_strerr(err)));
//      sock_set_errno(sock, err_to_errno(err));
//      free_socket(sock,0);//done_socket(sock);
//      return -1;
//    }
//    ret = (ssize_t)LWIP_MIN(LWIP_MIN(len, datagram_len), SSIZE_MAX);
//    if (fromlen) {
//      *fromlen = msg.msg_namelen;
//    }
//  }
//
//  sock_set_errno(sock, 0);
//  free_socket(sock,0);//done_socket(sock);
//  return ret;
//}

ssize_t
ida_lwip_sendto(int s, const void *data, size_t size, int flags,
            const struct sockaddr *to, socklen_t tolen)
{
  struct lwip_sock *sock;
  err_t err;
  u16_t short_size;
  u16_t remote_port;
  struct netbuf buf;

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (size > LWIP_MIN(0xFFFF, SSIZE_MAX)) {
    /* cannot fit into one datagram (at least for us) */
    sock_set_errno(sock, EMSGSIZE);
    return -1;
  }
  short_size = (u16_t)size;
  LWIP_UNUSED_ARG(tolen);

  /* initialize a buffer */
  buf.p = buf.ptr = NULL;
  if (to) {
    SOCKADDR_TO_IPADDR_PORT(to, &buf.addr, remote_port);
  } else {
    remote_port = 0;
    ip_addr_set_any(NETCONNTYPE_ISIPV6(netconn_type(sock->conn)), &buf.addr);
  }
  netbuf_fromport(&buf) = remote_port;


  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_sendto(%d, data=%p, short_size=%"U16_F", flags=0x%x to=",
                              s, data, short_size, flags));
  ip_addr_debug_print_val(SOCKETS_DEBUG, buf.addr);
  LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F"\n", remote_port));

  /* make the buffer point to the data that should be sent */
  err = netbuf_ref(&buf, data, short_size);
  if (err == ERR_OK) {
#if LWIP_IPV4 && LWIP_IPV6
    /* Dual-stack: Unmap IPv4 mapped IPv6 addresses */
    if (IP_IS_V6_VAL(buf.addr) && ip6_addr_isipv4mappedipv6(ip_2_ip6(&buf.addr))) {
      unmap_ipv4_mapped_ipv6(ip_2_ip4(&buf.addr), ip_2_ip6(&buf.addr));
      IP_SET_TYPE_VAL(buf.addr, IPADDR_TYPE_V4);
    }
#endif /* LWIP_IPV4 && LWIP_IPV6 */

    /* send the data */
    err = netconn_send(sock->conn, &buf);
  }

  /* deallocated the buffer */
  netbuf_free(&buf);

  sock_set_errno(sock, err_to_errno(err));
  return (err == ERR_OK ? short_size : -1);
}

/** Free a socket (under lock)
 *
 * @param sock the socket to free
 * @param is_tcp != 0 for TCP sockets, used to free lastdata
 * @param conn the socekt's netconn is stored here, must be freed externally
 * @param lastdata lastdata is stored here, must be freed externally
 */
static int
free_socket_locked(struct lwip_sock *sock, int is_tcp, struct netconn **conn,
                   union lwip_sock_lastdata *lastdata)
{
#if LWIP_NETCONN_FULLDUPLEX
  LWIP_ASSERT("sock->fd_used > 0", sock->fd_used > 0);
  sock->fd_used--;
  if (sock->fd_used > 0) {
    sock->fd_free_pending = LWIP_SOCK_FD_FREE_FREE | (is_tcp ? LWIP_SOCK_FD_FREE_TCP : 0);
    return 0;
  }
#else /* LWIP_NETCONN_FULLDUPLEX */
  LWIP_UNUSED_ARG(is_tcp);
#endif /* LWIP_NETCONN_FULLDUPLEX */

  *lastdata = sock->lastdata;
  sock->lastdata.pbuf = NULL;
  *conn = sock->conn;
  sock->conn = NULL;
  return 1;
}

/** Free a socket's leftover members.
 */
static void
free_socket_free_elements(int is_tcp, struct netconn *conn, union lwip_sock_lastdata *lastdata)
{
  if (lastdata->pbuf != NULL) {
    if (is_tcp) {
      pbuf_free(lastdata->pbuf);
    } else {
      netbuf_delete(lastdata->netbuf);
    }
  }
  if (conn != NULL) {
    /* netconn_prepare_delete() has already been called, here we only free the conn */
    netconn_delete(conn);
  }
}

/** Free a socket. The socket's netconn must have been
 * delete before!
 *
 * @param sock the socket to free
 * @param is_tcp != 0 for TCP sockets, used to free lastdata
 */
static void
free_socket(struct lwip_sock *sock, int is_tcp)
{
  int freed;
  struct netconn *conn;
  union lwip_sock_lastdata lastdata;
  SYS_ARCH_DECL_PROTECT(lev);

  /* Protect socket array */
  SYS_ARCH_PROTECT(lev);

  freed = free_socket_locked(sock, is_tcp, &conn, &lastdata);
  SYS_ARCH_UNPROTECT(lev);
  /* don't use 'sock' after this line, as another task might have allocated it */

  if (freed) {
    free_socket_free_elements(is_tcp, conn, &lastdata);
  }
}

static int _ida_free_socket(struct ida_lwip_sock *sock)
{
	/* Protect socket array */
	sock->id=0;
	sock->prio=0;
	sys_sem_free(sock->sem);
	sys_mbox_free(sock->mbox);
	ida_monitor_free(sock->monitor);

	if(sock->sem != NULL && sock->mbox != NULL && sock->monitor != NULL){ //todo: does this work?
			return -1;
	}

	/* detach it from the free list */
	socketFreeList = sock->next;

	/* put socket back into free list*/
	socketFreeList = sock;

	return 0;
}



int
ida_lwip_close(int s)
{
	IDA_LWIP_SOCKET_MGM *msg = NULL;
	struct ida_lwip_sock *sock;

	sock = get_socket(s);
	if (!sock) {
		return -1;
	}

	if(_ida_lwip_socketMgmAlloc(&msg) < 0)
		return -1;

	msg->type = SOCKET_MGM_CLOSE;
	msg->socket = s;
	msg->data = sock;

	sys_mbox_post(&socket_mgm_queue,msg);
	sys_arch_sem_wait(&msg->ackSem,0);

	return _ida_lwip_socketMgmFree(msg);




//  struct lwip_sock *sock;
//  int is_tcp = 0;
//  err_t err;
//
//  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_close(%d)\n", s));
//
//  sock = get_socket(s);
//  if (!sock) {
//    return -1;
//  }
//
//  if (sock->conn != NULL) {
//    is_tcp = NETCONNTYPE_GROUP(netconn_type(sock->conn)) == NETCONN_TCP;
//  } else {
//    LWIP_ASSERT("sock->lastdata == NULL", sock->lastdata.pbuf == NULL);
//  }
//
//#if LWIP_IGMP
//  /* drop all possibly joined IGMP memberships */
//  lwip_socket_drop_registered_memberships(s);
//#endif /* LWIP_IGMP */
//#if LWIP_IPV6_MLD
//  /* drop all possibly joined MLD6 memberships */
//  lwip_socket_drop_registered_mld6_memberships(s);
//#endif /* LWIP_IPV6_MLD */
//
//  err = netconn_prepare_delete(sock->conn);
//  if (err != ERR_OK) {
//    sock_set_errno(sock, err_to_errno(err));
//    return -1;
//  }
//
//  free_socket(sock, is_tcp);
//  set_errno(0);
//  return 0;
}

int
ida_lwip_connect(int s, const struct sockaddr *name, socklen_t namelen)
{
  struct lwip_sock *sock;
  err_t err;

  sock = get_socket(s);
  if (!sock) {
    return -1;
  }

  if (!SOCK_ADDR_TYPE_MATCH_OR_UNSPEC(name, sock)) {
    /* sockaddr does not match socket type (IPv4/IPv6) */
    sock_set_errno(sock, err_to_errno(ERR_VAL));
    return -1;
  }

  LWIP_UNUSED_ARG(namelen);
  if (name->sa_family == AF_UNSPEC) {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_connect(%d, AF_UNSPEC)\n", s));
    err = netconn_disconnect(sock->conn);
  } else {
    ip_addr_t remote_addr;
    u16_t remote_port;

    /* check size, family and alignment of 'name' */
    LWIP_ERROR("lwip_connect: invalid address", IS_SOCK_ADDR_LEN_VALID(namelen) &&
               IS_SOCK_ADDR_TYPE_VALID_OR_UNSPEC(name) && IS_SOCK_ADDR_ALIGNED(name),
               sock_set_errno(sock, err_to_errno(ERR_ARG)); done_socket(sock); return -1;);

    SOCKADDR_TO_IPADDR_PORT(name, &remote_addr, remote_port);
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_connect(%d, addr=", s));
    ip_addr_debug_print_val(SOCKETS_DEBUG, remote_addr);
    LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F")\n", remote_port));

#if LWIP_IPV4 && LWIP_IPV6
    /* Dual-stack: Unmap IPv4 mapped IPv6 addresses */
    if (IP_IS_V6_VAL(remote_addr) && ip6_addr_isipv4mappedipv6(ip_2_ip6(&remote_addr))) {
      unmap_ipv4_mapped_ipv6(ip_2_ip4(&remote_addr), ip_2_ip6(&remote_addr));
      IP_SET_TYPE_VAL(remote_addr, IPADDR_TYPE_V4);
    }
#endif /* LWIP_IPV4 && LWIP_IPV6 */

    err = netconn_connect(sock->conn, &remote_addr, remote_port);
  }

  if (err != ERR_OK) {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_connect(%d) failed, err=%d\n", s, err));
    sock_set_errno(sock, err_to_errno(err));
    done_socket(sock);
    return -1;
  }

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_connect(%d) succeeded\n", s));
  sock_set_errno(sock, 0);
  done_socket(sock);
  return 0;
}


/* Convert a netbuf's address data to struct sockaddr */
static int
lwip_sock_make_addr(struct netconn *conn, ip_addr_t *fromaddr, u16_t port,
                    struct sockaddr *from, socklen_t *fromlen)
{
  int truncated = 0;
  union sockaddr_aligned saddr;

  LWIP_UNUSED_ARG(conn);

  LWIP_ASSERT("fromaddr != NULL", fromaddr != NULL);
  LWIP_ASSERT("from != NULL", from != NULL);
  LWIP_ASSERT("fromlen != NULL", fromlen != NULL);

#if LWIP_IPV4 && LWIP_IPV6
  /* Dual-stack: Map IPv4 addresses to IPv4 mapped IPv6 */
  if (NETCONNTYPE_ISIPV6(netconn_type(conn)) && IP_IS_V4(fromaddr)) {
    ip4_2_ipv4_mapped_ipv6(ip_2_ip6(fromaddr), ip_2_ip4(fromaddr));
    IP_SET_TYPE(fromaddr, IPADDR_TYPE_V6);
  }
#endif /* LWIP_IPV4 && LWIP_IPV6 */

  IPADDR_PORT_TO_SOCKADDR(&saddr, fromaddr, port);
  if (*fromlen < saddr.sa.sa_len) {
    truncated = 1;
  } else if (*fromlen > saddr.sa.sa_len) {
    *fromlen = saddr.sa.sa_len;
  }
  MEMCPY(from, &saddr, *fromlen);
  return truncated;
}

#if LWIP_TCP
/* Helper function to get a tcp socket's remote address info */
static int
lwip_recv_tcp_from(struct lwip_sock *sock, struct sockaddr *from, socklen_t *fromlen, const char *dbg_fn, int dbg_s, ssize_t dbg_ret)
{
  if (sock == NULL) {
    return 0;
  }
  LWIP_UNUSED_ARG(dbg_fn);
  LWIP_UNUSED_ARG(dbg_s);
  LWIP_UNUSED_ARG(dbg_ret);

#if !SOCKETS_DEBUG
  if (from && fromlen)
#endif /* !SOCKETS_DEBUG */
  {
    /* get remote addr/port from tcp_pcb */
    u16_t port;
    ip_addr_t tmpaddr;
    netconn_getaddr(sock->conn, &tmpaddr, &port, 0);
    LWIP_DEBUGF(SOCKETS_DEBUG, ("%s(%d):  addr=", dbg_fn, dbg_s));
    ip_addr_debug_print_val(SOCKETS_DEBUG, tmpaddr);
    LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F" len=%d\n", port, (int)dbg_ret));
    if (from && fromlen) {
      return lwip_sock_make_addr(sock->conn, &tmpaddr, port, from, fromlen);
    }
  }
  return 0;
}
#endif

/* Helper function to receive a netbuf from a udp or raw netconn.
 * Keeps sock->lastdata for peeking.
 */
static err_t
lwip_recvfrom_udp_raw(struct lwip_sock *sock, int flags, struct msghdr *msg, u16_t *datagram_len, int dbg_s)
{
  struct netbuf *buf;
  u8_t apiflags;
  err_t err;
  u16_t buflen, copylen, copied;
  int i;

  LWIP_UNUSED_ARG(dbg_s);
  LWIP_ERROR("lwip_recvfrom_udp_raw: invalid arguments", (msg->msg_iov != NULL) || (msg->msg_iovlen <= 0), return ERR_ARG;);

  if (flags & MSG_DONTWAIT) {
    apiflags = NETCONN_DONTBLOCK;
  } else {
    apiflags = 0;
  }

  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom_udp_raw[UDP/RAW]: top sock->lastdata=%p\n", (void *)sock->lastdata.netbuf));
  /* Check if there is data left from the last recv operation. */
  buf = sock->lastdata.netbuf;
  if (buf == NULL) {
    /* No data was left from the previous operation, so we try to get
        some from the network. */
    err = netconn_recv_udp_raw_netbuf_flags(sock->conn, &buf, apiflags);
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom_udp_raw[UDP/RAW]: netconn_recv err=%d, netbuf=%p\n",
                                err, (void *)buf));

    if (err != ERR_OK) {
      return err;
    }
    LWIP_ASSERT("buf != NULL", buf != NULL);
    sock->lastdata.netbuf = buf;
  }
  buflen = buf->p->tot_len;
  LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom_udp_raw: buflen=%"U16_F"\n", buflen));

  copied = 0;
  /* copy the pbuf payload into the iovs */
  for (i = 0; (i < msg->msg_iovlen) && (copied < buflen); i++) {
    u16_t len_left = (u16_t)(buflen - copied);
    if (msg->msg_iov[i].iov_len > len_left) {
      copylen = len_left;
    } else {
      copylen = (u16_t)msg->msg_iov[i].iov_len;
    }

    /* copy the contents of the received buffer into
        the supplied memory buffer */
    pbuf_copy_partial(buf->p, (u8_t *)msg->msg_iov[i].iov_base, copylen, copied);
    copied = (u16_t)(copied + copylen);
  }

  /* Check to see from where the data was.*/
#if !SOCKETS_DEBUG
  if (msg->msg_name && msg->msg_namelen)
#endif /* !SOCKETS_DEBUG */
  {
    LWIP_DEBUGF(SOCKETS_DEBUG, ("lwip_recvfrom_udp_raw(%d):  addr=", dbg_s));
    ip_addr_debug_print_val(SOCKETS_DEBUG, *netbuf_fromaddr(buf));
    LWIP_DEBUGF(SOCKETS_DEBUG, (" port=%"U16_F" len=%d\n", netbuf_fromport(buf), copied));
    if (msg->msg_name && msg->msg_namelen) {
      lwip_sock_make_addr(sock->conn, netbuf_fromaddr(buf), netbuf_fromport(buf),
                          (struct sockaddr *)msg->msg_name, &msg->msg_namelen);
    }
  }

  /* Initialize flag output */
  msg->msg_flags = 0;

  if (msg->msg_control) {
    u8_t wrote_msg = 0;
#if LWIP_NETBUF_RECVINFO
    /* Check if packet info was recorded */
    if (buf->flags & NETBUF_FLAG_DESTADDR) {
      if (IP_IS_V4(&buf->toaddr)) {
#if LWIP_IPV4
        if (msg->msg_controllen >= CMSG_SPACE(sizeof(struct in_pktinfo))) {
          struct cmsghdr *chdr = CMSG_FIRSTHDR(msg); /* This will always return a header!! */
          struct in_pktinfo *pkti = (struct in_pktinfo *)CMSG_DATA(chdr);
          chdr->cmsg_level = IPPROTO_IP;
          chdr->cmsg_type = IP_PKTINFO;
          chdr->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
          pkti->ipi_ifindex = buf->p->if_idx;
          inet_addr_from_ip4addr(&pkti->ipi_addr, ip_2_ip4(netbuf_destaddr(buf)));
          msg->msg_controllen = CMSG_SPACE(sizeof(struct in_pktinfo));
          wrote_msg = 1;
        } else {
          msg->msg_flags |= MSG_CTRUNC;
        }
#endif /* LWIP_IPV4 */
      }
    }
#endif /* LWIP_NETBUF_RECVINFO */

    if (!wrote_msg) {
      msg->msg_controllen = 0;
    }
  }

  /* If we don't peek the incoming message: zero lastdata pointer and free the netbuf */
  if ((flags & MSG_PEEK) == 0) {
    sock->lastdata.netbuf = NULL;
    netbuf_delete(buf);
  }
  if (datagram_len) {
    *datagram_len = buflen;
  }
  return ERR_OK;
}





/**
 * Callback registered in the netconn layer for each socket-netconn.
 * Processes recvevent (data available) and wakes up tasks waiting for select.
 *
 * @note for LWIP_TCPIP_CORE_LOCKING any caller of this function
 * must have the core lock held when signaling the following events
 * as they might cause select_list_cb to be checked:
 *   NETCONN_EVT_RCVPLUS
 *   NETCONN_EVT_SENDPLUS
 *   NETCONN_EVT_ERROR
 * This requirement will be asserted in select_check_waiters()
 */
static void
event_callback(struct netconn *conn, enum netconn_evt evt, u16_t len)
{
  int s, check_waiters;
  struct lwip_sock *sock;
  SYS_ARCH_DECL_PROTECT(lev);

  LWIP_UNUSED_ARG(len);

  /* Get socket */
  if (conn) {
    s = conn->socket;
    if (s < 0) {
      /* Data comes in right away after an accept, even though
       * the server task might not have created a new socket yet.
       * Just count down (or up) if that's the case and we
       * will use the data later. Note that only receive events
       * can happen before the new socket is set up. */
      SYS_ARCH_PROTECT(lev);
      if (conn->socket < 0) {
        if (evt == NETCONN_EVT_RCVPLUS) {
          /* conn->socket is -1 on initialization
             lwip_accept adjusts sock->recvevent if conn->socket < -1 */
          conn->socket--;
        }
        SYS_ARCH_UNPROTECT(lev);
        return;
      }
      s = conn->socket;
      SYS_ARCH_UNPROTECT(lev);
    }

    sock = get_socket(s);
    if (!sock) {
      return;
    }
  } else {
    return;
  }

  check_waiters = 1;
  SYS_ARCH_PROTECT(lev);
  /* Set event as required */
  switch (evt) {
    case NETCONN_EVT_RCVPLUS:
      sock->rcvevent++;
      if (sock->rcvevent > 1) {
        check_waiters = 0;
      }
      break;
    case NETCONN_EVT_RCVMINUS:
      sock->rcvevent--;
      check_waiters = 0;
      break;
    case NETCONN_EVT_SENDPLUS:
      if (sock->sendevent) {
        check_waiters = 0;
      }
      sock->sendevent = 1;
      break;
    case NETCONN_EVT_SENDMINUS:
      sock->sendevent = 0;
      check_waiters = 0;
      break;
    case NETCONN_EVT_ERROR:
      sock->errevent = 1;
      break;
    default:
      LWIP_ASSERT("unknown event", 0);
      break;
  }

  if (sock->select_waiting && check_waiters) {
    /* Save which events are active */
    int has_recvevent, has_sendevent, has_errevent;
    has_recvevent = sock->rcvevent > 0;
    has_sendevent = sock->sendevent != 0;
    has_errevent = sock->errevent != 0;
    SYS_ARCH_UNPROTECT(lev);
    /* Check any select calls waiting on this socket */
    select_check_waiters(s, has_recvevent, has_sendevent, has_errevent);
  } else {
    SYS_ARCH_UNPROTECT(lev);
  }
  done_socket(sock);
}

const char *
ida_lwip_inet_ntop(int af, const void *src, char *dst, socklen_t size)
{
  const char *ret = NULL;
  int size_int = (int)size;
  if (size_int < 0) {
    set_errno(ENOSPC);
    return NULL;
  }
  switch (af) {
#if LWIP_IPV4
    case AF_INET:
      ret = ip4addr_ntoa_r((const ip4_addr_t *)src, dst, size_int);
      if (ret == NULL) {
        set_errno(ENOSPC);
      }
      break;
#endif
#if LWIP_IPV6
    case AF_INET6:
      ret = ip6addr_ntoa_r((const ip6_addr_t *)src, dst, size_int);
      if (ret == NULL) {
        set_errno(ENOSPC);
      }
      break;
#endif
    default:
      set_errno(EAFNOSUPPORT);
      break;
  }
  return ret;
}

int
ida_lwip_inet_pton(int af, const char *src, void *dst)
{
  int err;
  switch (af) {
#if LWIP_IPV4
    case AF_INET:
      err = ip4addr_aton(src, (ip4_addr_t *)dst);
      break;
#endif
#if LWIP_IPV6
    case AF_INET6: {
      /* convert into temporary variable since ip6_addr_t might be larger
         than in6_addr when scopes are enabled */
      ip6_addr_t addr;
      err = ip6addr_aton(src, &addr);
      if (err) {
        memcpy(dst, &addr.addr, sizeof(addr.addr));
      }
      break;
    }
#endif
    default:
      err = -1;
      set_errno(EAFNOSUPPORT);
      break;
  }
  return err;
}

