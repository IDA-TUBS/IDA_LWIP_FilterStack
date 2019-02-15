/*
 * Copyright (c) 2001,2002 Florian Schulze.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the authors nor the names of the contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * test.c - This file is part of lwIP test
 *
 */

/* C runtime includes */
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

/* lwIP core includes */
#include "lwip/opt.h"

#include "lwip/sys.h"
#include "lwip/timeouts.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/init.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "lwip/api.h"

#include "lwip/tcp.h"
#include "lwip/udp.h"
#include "lwip/dns.h"
#include "lwip/dhcp.h"
#include "lwip/autoip.h"

/* lwIP netif includes */
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include "default_netif.h"

#include "udpecho.h"

/* include the port-dependent configuration */
#include "lwipcfg.h"

/* This function initializes all network interfaces */
static void
test_netif_init(void)
{
  ip4_addr_t ipaddr, netmask, gw;

  ip4_addr_set_zero(&gw);
  ip4_addr_set_zero(&ipaddr);
  ip4_addr_set_zero(&netmask);

  LWIP_PORT_INIT_GW(&gw);
  LWIP_PORT_INIT_IPADDR(&ipaddr);
  LWIP_PORT_INIT_NETMASK(&netmask);

  init_default_netif(&ipaddr, &netmask, &gw);
  netif_set_up(netif_default);
}

/* This function initializes this lwIP test. When NO_SYS=1, this is done in
 * the main_loop context (there is no other one), when NO_SYS=0, this is done
 * in the tcpip_thread context */
static void
test_init(void * arg)
{ /* remove compiler warning */
  sys_sem_t *init_sem;
  LWIP_ASSERT("arg != NULL", arg != NULL);
  init_sem = (sys_sem_t*)arg;
  /* init randomizer again (seed per thread) */
  srand((unsigned int)time(0));

  /* init network interfaces */
  test_netif_init();

  sys_sem_signal(init_sem);
}

/* This is somewhat different to other ports: we have a main loop here:
 * a dedicated task that waits for packets to arrive. This would normally be
 * done from interrupt context with embedded hardware, but we don't get an
 * interrupt in windows for that :-) */
static void
main_loop(void)
{
  err_t err;
  sys_sem_t init_sem;

  /* initialize lwIP stack, network interfaces and applications */
  err = sys_sem_new(&init_sem, 0);
  LWIP_ASSERT("failed to create init_sem", err == ERR_OK);
  LWIP_UNUSED_ARG(err);
  tcpip_init(test_init, &init_sem);
  /* we have to wait for initialization to finish before
   * calling update_adapter()! */
  sys_sem_wait(&init_sem);
  sys_sem_free(&init_sem);

  udpecho_init();

  /* MAIN LOOP for driver update (and timers if NO_SYS) */
  while (!LWIP_EXAMPLE_APP_ABORT()) {

  /* MAIN LOOP for driver update (and timers if NO_SYS) */
  while (1) {
    default_netif_poll();
  }

  default_netif_shutdown();
}

int main(void)
{

  /* no stdio-buffering, please! */
  setvbuf(stdout, NULL,_IONBF, 0);

  main_loop();

  return 0;
}
