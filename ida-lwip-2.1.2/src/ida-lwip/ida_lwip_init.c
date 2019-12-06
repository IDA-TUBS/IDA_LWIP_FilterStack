/*
 * ida_lwip_init.c
 *
 *  Created on: 16.09.2019
 *      Author: kaige
 */

#include "lwip/sys.h"
#include "xil_mpu.h"
#include "xreg_cortexr5.h"
#include "lwip/init.h"

#include "ida-lwip/ida_lwip_filter.h"
#include "ida-lwip/ida_lwip_queue.h"
#include "ida-lwip/ida_lwip_prio_queue.h"
#include "lwip/sockets.h"

#ifndef IDA_LWIP_MEMORY_REGION_BASE
#error "Memory Region base for lwip data not defined"
#endif

#ifndef IDA_LWIP_MEMORY_REGION_SIZE
#error "Memory Region size for lwip data not defined"
#endif


/*
 * function to initialize all modules
 *
 * */
void ida_lwip_init(struct netif *netif){
	Xil_SetMPURegion(IDA_LWIP_MEMORY_REGION_BASE, IDA_LWIP_MEMORY_REGION_SIZE, NORM_SHARED_NCACHE | PRIV_RW_USER_RW);

	lwip_init();

	//ida_monitor_init();
	ida_lwip_queue_init();
	ida_lwip_prioQueueInit();

	ida_filter_init(netif);

	ida_lwip_initSockets();
}
