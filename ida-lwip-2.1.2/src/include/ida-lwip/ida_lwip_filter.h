/*
 * ida_lwip_filter.h
 *
 *  Created on: Jun 11, 2019
 *      Author: noras
 */

#ifndef SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_FILTER_H_
#define SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_FILTER_H_

#include "lwip/sys.h"
#include "lwip/pbuf.h"
#include "lwip/etharp.h"
#include "lwip/ip.h"

#define IDA_FILTER_MBOX_SIZE 8
#define MBOX_SEM_TIMEOUT 0 //wait forever

err_t ida_filter_enqueue_pkt(struct pbuf *p, u8_t prio);
void ida_filter_init(struct netif *netif);


#endif /* SRC_MODULES_LWIP_IDA_LWIP_2_1_2_SRC_INCLUDE_IDA_LWIP_IDA_LWIP_FILTER_H_ */
