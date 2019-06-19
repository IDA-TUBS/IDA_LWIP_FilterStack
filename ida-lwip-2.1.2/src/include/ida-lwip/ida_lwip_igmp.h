/*
 * ida_lwip_igmp.h
 *
 *  Created on: 19.06.2019
 *      Author: kaige
 */

#ifndef SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_IGMP_H_
#define SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_IGMP_H_

#include "lwip/opt.h"
#include "lwip/ip_addr.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"

/* Compatibility defines (don't use for new code) */
#define IGMP_DEL_MAC_FILTER            NETIF_DEL_MAC_FILTER
#define IGMP_ADD_MAC_FILTER            NETIF_ADD_MAC_FILTER

/**
 * igmp group structure - there is
 * a list of groups for each interface
 * these should really be linked from the interface, but
 * if we keep them separate we will not affect the lwip original code
 * too much
 *
 * There will be a group for the all systems group address but this
 * will not run the state machine as it is used to kick off reports
 * from all the other groups
 */
struct igmp_group {
  /** next link */
  struct igmp_group *next;
  /** multicast address */
  ip4_addr_t         group_address;
  /** signifies we were the last person to report */
  u8_t               last_reporter_flag;
  /** current state of the group */
  u8_t               group_state;
  /** timer for reporting, negative is OFF */
  u16_t              timer;
  /** counter of simultaneous uses */
  u8_t               use;
};

/*  Prototypes */
void   igmp_init(void);
struct igmp_group *ida_lwip_igmp_lookfor_group(struct netif *ifp, const ip4_addr_t *addr);

#endif /* SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_IGMP_H_ */
