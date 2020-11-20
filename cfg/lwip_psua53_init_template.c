/*
 ============================================================================
 Name        : main.c
 Author      : Kai-Björn Gemlau, IDA TU-Braunschweig 
 Version     :
 Copyright   : Copyright belongs to the authors
 ============================================================================
 */

#include <stdio.h>
#include "cpu_core.h"
#include "ucos_ii.h"
#include "ucos_bsp.h"

#include <ucos_int.h>
#include "lwip/init.h"
#include "lwip/inet.h"
#include "lwip/opt.h"
#include "lwip/ip_addr.h"

#include "lwip/tcpip.h"
#include "lwip/dhcp.h"

#include "netif/xemacpsif.h"
#include "netif/xadapter.h"
#include "netif/xemacps_ieee1588.h"
#include "ptpd.h"

extern void startAPP(void);

struct netif EthDev;

#define MAIN_TASK_STACK_SIZE 192
static CPU_STK mainTaskStk[MAIN_TASK_STACK_SIZE];

int printf(const char *x, ...) {return 0;}
int puts(const char *x) {return 0;}
void xil_printf( const char8 *ctrl1, ...){}

void initializeNetwork(void) {
	ip_addr_t netmask = IPADDR4_INIT_BYTES(255,255,255,0);

	ip_addr_t ipaddrGW = IPADDR4_INIT_BYTES(192,168,178,1);
	ip_addr_t ipaddrBoard1 = IPADDR4_INIT_BYTES(192,168,178,10);
	ip_addr_t ipaddrBoard2 = IPADDR4_INIT_BYTES(192,168,178,11);
	ip_addr_t ipaddrHost1 = IPADDR4_INIT_BYTES(192,168,178,20);
	ip_addr_t ipaddrHost2 = IPADDR4_INIT_BYTES(192,168,178,21);
	ip_addr_t ipaddrHost3 = IPADDR4_INIT_BYTES(192,168,178,22);
	ip_addr_t ipaddrHost4 = IPADDR4_INIT_BYTES(192,168,178,23);
	ip_addr_t ipaddrNone = IPADDR4_INIT_BYTES(0,0,0,0);

	struct eth_addr macBoard1 = ETH_ADDR(0x00, 0x0a, 0x35, 0x00, 0x22, 0x01);
	struct eth_addr macBoard2 = ETH_ADDR(0x00, 0x0a, 0x35, 0x00, 0x22, 0x02);
	struct eth_addr macHost1 = ETH_ADDR(0xe4, 0x54, 0xe8, 0x5a, 0xc3, 0xad);
	struct eth_addr macHost2 = ETH_ADDR(0x90, 0xb1, 0x1c, 0x65, 0x60, 0x6e);
	struct eth_addr macHost3 = ETH_ADDR(0x00, 0x1b, 0x21, 0x74, 0x35, 0x59);
	struct eth_addr macHost4 = ETH_ADDR(0x00, 0x1b, 0x21, 0x3a, 0x60, 0x63);

	struct eth_addr macGW = ETH_ADDR(0xe8, 0x65, 0x49, 0x5c, 0x06, 0x38);

	tcpip_init(NULL, NULL);

	/* Add network interface to the netif_list, and set it as default */
#if BOARD == 1
	/* set mac address */
	if (!xemac_add(&EthDev, &ipaddrBoard1, &netmask, &ipaddrGW, macBoard1.addr,XPAR_XEMACPS_0_BASEADDR)) {
		xil_printf("Error adding N/W interface\r\n");
		return;
	}
#elif BOARD == 2
	/* set mac address */
	if (!xemac_add(&EthDev, &ipaddrBoard2, &netmask, &ipaddrGW, macBoard2.addr,XPAR_XEMACPS_0_BASEADDR)) {
		xil_printf("Error adding N/W interface\r\n");
		return;
	}
#else
#error "Please specify board number"
#endif

	netif_set_up(&EthDev);
	netif_set_default(&EthDev);

	etharp_add_static_entry(&ipaddrBoard1,&macBoard1);
	etharp_add_static_entry(&ipaddrBoard2,&macBoard2);
	etharp_add_static_entry(&ipaddrHost1,&macHost1);
	etharp_add_static_entry(&ipaddrHost2,&macHost2);
	etharp_add_static_entry(&ipaddrHost3,&macHost3);
	etharp_add_static_entry(&ipaddrHost4,&macHost4);
	etharp_add_static_entry(&ipaddrGW,&macGW);


	sys_thread_new("xemacif_input_thread",(void (*)(void*)) xemacif_input_thread, &EthDev, 512,	TCPIP_THREAD_PRIO + 1);

	ptpd_init();
}

void mainTask(void * p_arg) {
	(void) p_arg;

	UCOS_StartupInit();
	UCOS_TmrTickInit(OS_TICKS_PER_SEC);

	OSStatInit();

	initializeNetwork();

	startAPP();

	OSTaskSuspend(OS_PRIO_SELF);

	//Should never get here
	while (1);;
}

int main(void) {
	UCOS_LowLevelInit();
	CPU_Init();
	OSInit();

	OSTaskCreateExt( mainTask,
					NULL,
					&mainTaskStk[MAIN_TASK_STACK_SIZE - 1],
					0,
					0,
					&mainTaskStk[0],
					MAIN_TASK_STACK_SIZE,
					DEF_NULL,
					(OS_TASK_OPT_STK_CLR | OS_TASK_OPT_STK_CHK));

	OSTaskNameSet(0,(INT8U *)"mainTask",NULL);
	OSStart();

	//Should never get here
	while (1);;
}
