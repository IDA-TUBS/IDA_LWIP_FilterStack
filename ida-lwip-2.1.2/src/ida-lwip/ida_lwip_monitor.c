/*
 * ida_lwip_monitor.c
 *
 *  Created on: 12.06.2019
 *      Author: kaige
 */

#include "ida-lwip/ida_lwip_monitor.h"
#include "lwip/memp.h"
#include "xemacps.h"

LWIP_MEMPOOL_DECLARE(RX_POOL, 20, sizeof(MONITORED_PBUF_T), "Monitored Pbuf_Pool");
LWIP_MEMPOOL_DECLARE(RX_DATA_POOL, 20, XEMACPS_MAX_FRAME_SIZE, "Rx Data Pool");
LWIP_MEMPOOL_DECLARE(MONITOR_POOL, 10, sizeof(PBUF_MONITOR_T), "Monitor Pool");

/*
 * function to free a monitored pbuf
 *
 * @param p: pbuf to free
 *
 * */
static void _ida_monitored_pbuf_free(struct pbuf *p)
{
	CPU_SR cpu_sr;
	OS_ENTER_CRITICAL();
	MONITORED_PBUF_T* my_pbuf = (MONITORED_PBUF_T*)p;
	if(my_pbuf->monitor_ref != (PBUF_MONITOR_T*)NULL){
		my_pbuf->monitor_ref->counter--;
		my_pbuf->monitor_ref = NULL;
	}
	void *my_pbuf_data = my_pbuf->p.pbuf.payload;
	LWIP_MEMPOOL_FREE(RX_DATA_POOL, my_pbuf_data);
	LWIP_MEMPOOL_FREE(RX_POOL, my_pbuf);
	OS_EXIT_CRITICAL();
}

/*
 * function to allocate a monitored pbuf
 *
 * @param length: length of buffer to allocate
 *
 * @return p: allocated pbuf
 *
 * */
struct pbuf * ida_monitored_pbuf_alloc(u16_t length){

	MONITORED_PBUF_T* my_pbuf  = (MONITORED_PBUF_T*)LWIP_MEMPOOL_ALLOC(RX_POOL);
	void *my_pbuf_data = LWIP_MEMPOOL_ALLOC(RX_DATA_POOL);
	my_pbuf->p.custom_free_function = _ida_monitored_pbuf_free;
	my_pbuf->monitor_ref         = NULL;

	struct pbuf* p = pbuf_alloced_custom(PBUF_RAW,
		length,
		PBUF_POOL,
		&my_pbuf->p,
		my_pbuf_data,
		XEMACPS_MAX_FRAME_SIZE);

	return p;
}

/*
 * function to initialize the memory pools for monitors, pbufs and received data
 *
 * */
void ida_monitor_init(void){
	LWIP_MEMPOOL_INIT(RX_POOL);
	LWIP_MEMPOOL_INIT(RX_DATA_POOL);
	LWIP_MEMPOOL_INIT(MONITOR_POOL);
}

/*
 * function to set the trigger for a certain monitor
 *
 * @param monitor: monitor to edit
 * @param trigger: new trigger value
 *
 * */
void ida_monitor_set_trigger(PBUF_MONITOR_T *monitor, u16_t trigger){
	if(monitor != (PBUF_MONITOR_T*)NULL){
		monitor->trigger = trigger;
	}
}

/*
 * function to check whether new pbuf triggers the monitor
 *
 * @param p: new pbuf
 * @param monitor: monitor to check
 *
 * @return success/fail (1/0)
 *
 * */
int ida_monitor_check(struct pbuf *p, PBUF_MONITOR_T *monitor){
	CPU_SR cpu_sr;
	int result = 0;

	if(p->flags != PBUF_FLAG_IS_CUSTOM)
		return 0;

	MONITORED_PBUF_T *my_pbuf = (MONITORED_PBUF_T*)p;
	if(my_pbuf->monitor_ref != (PBUF_MONITOR_T*)NULL)
		return 0;

	OS_ENTER_CRITICAL();
	if(monitor->counter < monitor->trigger){
		monitor->counter++;
		my_pbuf->monitor_ref = monitor;
		result = 1;
	}
	OS_EXIT_CRITICAL();
	return result;
}

/*
 * function to allocate a monitor
 *
 * @param trigger: trigger value the initialize the monitor with
 *
 * @return new monitor
 *
 * */
PBUF_MONITOR_T * ida_monitor_alloc(u16_t trigger){
	PBUF_MONITOR_T *monitor = LWIP_MEMPOOL_ALLOC(MONITOR_POOL);
	if(monitor == NULL)
		return NULL;
	monitor->counter = 0;
	monitor->trigger = trigger;
	return monitor;
}

/*
 * function to free a monitor
 *
 * @param monitor: monitor to free
 *
 * */
void ida_monitor_free(PBUF_MONITOR_T *monitor){
	if(monitor != NULL){
		monitor->counter = 0;
		monitor->trigger = 0;
		LWIP_MEMPOOL_FREE(MONITOR_POOL, monitor);
	}

}


