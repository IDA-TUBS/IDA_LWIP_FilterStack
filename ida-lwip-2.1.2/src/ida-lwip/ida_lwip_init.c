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


#include "lwip/sys.h"
#include "xil_mpu.h"
#include "xreg_cortexr5.h"
#include "lwip/init.h"

#include "ida-lwip/ida_lwip_filter.h"
#include "ida-lwip/ida_lwip_queue.h"
#include "ida-lwip/ida_lwip_prio_queue.h"
#include "ida_lwip_virtEth.h"
#include "lwip/sockets.h"

#include "xil_mpu.h"

#ifndef IDA_LWIP_MEMORY_REGION_BASE
#error "Memory Region base for lwip data not defined"
#endif

#ifndef IDA_LWIP_MEMORY_REGION_SIZE
#error "Memory Region size for lwip data not defined"
#endif


static XMpu_Config _mpuconfig;

static void _ida_lwip_init_mpu(INTPTR addr, u64 size, u32 attrib){
	int i = 0;
	Xil_GetMPUConfig(_mpuconfig);
	for(i = 0; i < MAX_POSSIBLE_MPU_REGS; i++){
		if(_mpuconfig[i].BaseAddress == addr && _mpuconfig[i].Size == size && _mpuconfig[i].RegionStatus == MPU_REG_ENABLED){
			/* Found the exact mpu region we can modify */
			Xil_DisableMPURegionByRegNum(i);
			Xil_SetMPURegionByRegNum(i, addr, size, attrib);
			return;
		}
	}
	/* Region not exactly matched, add new one */
	Xil_SetMPURegion(addr, size, attrib);
}

/*
 * function to initialize all modules
 *
 * */
void ida_lwip_init(struct netif *netif){
	_ida_lwip_init_mpu(IDA_LWIP_MEMORY_REGION_BASE, IDA_LWIP_MEMORY_REGION_SIZE, NORM_SHARED_NCACHE | PRIV_RW_USER_RW);
	_ida_lwip_init_mpu(IDA_LWIP_VIRT_ETH_MEM_BASE, IDA_LWIP_VIRT_ETH_MEM_SIZE, NORM_SHARED_NCACHE | PRIV_RW_USER_RW);

	lwip_init();

	//ida_monitor_init();
	ida_lwip_queue_init();
	ida_lwip_prioQueueInit();

	ida_filter_init(netif);

	ida_lwip_initSockets();
}
