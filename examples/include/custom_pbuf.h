/*
 * custom_pbuf.h
 *
 *  Created on: May 17, 2019
 *      Author: noras
 */

#ifndef EXAMPLES_INCLUDE_CUSTOM_PBUF_H_
#define EXAMPLES_INCLUDE_CUSTOM_PBUF_H_

#include "lwip/pbuf.h"
#include "lwip/opt.h"


#define MAX_BUF_FOR_STACK		5
#define POOL_SIZE				16
#define PBUF_SIZE 				1518

uint8_t buf_counter_for_stack;

typedef struct my_custom_pbuf
{
   struct pbuf_custom p;
   void* payload_memp;
   int owned_by_classic;
} my_custom_pbuf_t;

my_custom_pbuf_t* my_pbuf_alloc_custom(pbuf_layer layer, u16_t length);
void my_pbuf_free_custom(void* p);
void custom_pbuf_init();
void dummy_to_classic_stack(struct pbuf*);
void increase_obc_cnt();
//void eth_rx_irq();


#endif /* EXAMPLES_INCLUDE_CUSTOM_PBUF_H_ */
