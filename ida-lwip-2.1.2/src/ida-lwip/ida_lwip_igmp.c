/*
 * ida_lwip_igmp.c
 *
 *  Created on: 19.06.2019
 *      Author: kaige
 */

#include "lwip/opt.h"
#include "ida-lwip/ida_lwip_igmp.h"
#include "lwip/ip.h"

struct igmp_group *igmp_groups;

/**
 * Search for a group in the netif's igmp group list
 *
 * @param ifp the network interface for which to look
 * @param addr the group ip address to search for
 * @return a struct igmp_group* if the group has been found,
 *         NULL if the group wasn't found.
 */
struct igmp_group *
ida_lwip_igmp_lookfor_group(struct netif *ifp, const ip4_addr_t *addr)
{
  struct igmp_group *group = igmp_groups;			// todo: thread-safe ?? -> makes use of netif->client_data

  while (group != NULL) {
    if (ip4_addr_cmp(&(group->group_address), addr)) {
      return group;
    }
    group = group->next;
  }

  /* to be clearer, we return NULL here instead of
   * 'group' (which is also NULL at this point).
   */
  return NULL;
}
