/*
 * custom_pbuf.c
 *
 *  Created on: May 17, 2019
 *      Author: noras
 */

#include "custom_pbuf.h"
#include "lwip/memp.h"
#include "lwip/debug.h"

LWIP_MEMPOOL_DECLARE(CUSTOM_POOL, POOL_SIZE, sizeof(my_custom_pbuf_t), "Zero-copy RX PBUF pool");
LWIP_MEMPOOL_DECLARE(MEM_POOL, POOL_SIZE, PBUF_SIZE, "Zero-copy RX MEM pool");

void my_pbuf_free_custom(void* p)
{
  my_custom_pbuf_t* my_pbuf = (my_custom_pbuf_t*)p;
  //LOCK_INTERRUPTS();
  //free_rx_dma_descriptor(my_pbuf->dma_descriptor);
  LWIP_MEMPOOL_FREE(MEM_POOL, my_pbuf->payload_memp);
  LWIP_MEMPOOL_FREE(CUSTOM_POOL, my_pbuf);
  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("DEBUG: my_pbuf_free_custom\n"));
  if(my_pbuf->owned_by_classic != 0){
	  buf_counter_for_stack = buf_counter_for_stack - 1;
	  LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("DEBUG: (-) current number of pbufs owned by stack:%"U16_F"\n", buf_counter_for_stack));
  }
  //UNLOCK_INTERRUPTS();
}

my_custom_pbuf_t* my_pbuf_alloc_custom(pbuf_layer layer, u16_t length){
	my_custom_pbuf_t* my_pbuf  = (my_custom_pbuf_t*)LWIP_MEMPOOL_ALLOC(CUSTOM_POOL);
	my_pbuf->payload_memp = (void*)LWIP_MEMPOOL_ALLOC(MEM_POOL);
	my_pbuf->p.custom_free_function = my_pbuf_free_custom;
	my_pbuf->owned_by_classic = 0;

	struct pbuf* p = pbuf_alloced_custom(layer,PBUF_POOL_BUFSIZE,PBUF_REF,&my_pbuf->p,my_pbuf->payload_memp,PBUF_POOL_BUFSIZE);

	LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("DEBUG: my_pbuf_alloc_custom\n"));

	buf_counter_for_stack++;
	return my_pbuf;
}

void custom_pbuf_init(){

	LWIP_MEMPOOL_INIT(CUSTOM_POOL);
	LWIP_MEMPOOL_INIT(MEM_POOL);

}

void increase_obc_cnt(){
	if (buf_counter_for_stack < 255){
		buf_counter_for_stack++;
		LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("DEBUG: (+) current number of pbufs owned by stack:%"U16_F"\n", buf_counter_for_stack));
	}
}

void dummy_to_classic_stack(struct pbuf* pbuf){
	LWIP_DEBUGF(PBUF_DEBUG | LWIP_DBG_TRACE, ("DEBUG: buffer now owned by stack\n"));
}

//void eth_rx_irq()
//{
//  dma_descriptor*   dma_desc = get_RX_DMA_descriptor_from_ethernet();
//  my_custom_pbuf_t* my_pbuf  = (my_custom_pbuf_t*)LWIP_MEMPOOL_ALLOC(RX_POOL);
//  my_pbuf->p.custom_free_function = my_pbuf_free_custom;
//  my_pbuf->dma_descriptor         = dma_desc;
//  invalidate_cpu_cache(dma_desc->rx_data, dma_desc->rx_length);
//
//  struct pbuf* p = pbuf_alloced_custom(PBUF_RAW,
//     dma_desc->rx_length,
//     PBUF_REF,
//     &my_pbuf->p,
//     dma_desc->rx_data,
//     dma_desc->max_buffer_size);
//  if(netif->input(p, netif) != ERR_OK) {
//    pbuf_free(p);
//  }
//}


