# LwIP Repository
***

This is the LwIP Repository from the Institute of Computer and NetworkEngineering (IDA) at TU Braunschweig.
It consists of two parts

  * The "original" LwIP Stack from Adam Dunkels 
  * A modified Filter-Stack designed at IDA

The folder-structure is as follows

  * ida-lwip-2.1.2: The IDA-LwIP
  * lwip-2.1.2: The "classic" LwIP
  * ports/os: OS ports, currently for MicroC-OS 2 and Unix
  * ports/hw: Ethernet Ports, currently for zynq7000 (deprecated), Zynq Ultrascale (maintained) and STM32F7 (deprecated)
  * cfg: Examples for the lwipopts.h config file

The following contains the README and License Files from LwIP and IDA-LwIP

### LwIP:
INTRODUCTION

lwIP is a small independent implementation of the TCP/IP protocol suite.

The focus of the lwIP TCP/IP implementation is to reduce the RAM usage
while still having a full scale TCP. This making lwIP suitable for use
in embedded systems with tens of kilobytes of free RAM and room for
around 40 kilobytes of code ROM.

lwIP was originally developed by Adam Dunkels at the Computer and Networks
Architectures (CNA) lab at the Swedish Institute of Computer Science (SICS)
and is now developed and maintained by a worldwide network of developers.

FEATURES

  * IP (Internet Protocol, IPv4 and IPv6) including packet forwarding over
    multiple network interfaces
  * ICMP (Internet Control Message Protocol) for network maintenance and debugging
  * IGMP (Internet Group Management Protocol) for multicast traffic management
  * MLD (Multicast listener discovery for IPv6). Aims to be compliant with 
    RFC 2710. No support for MLDv2
  * ND (Neighbor discovery and stateless address autoconfiguration for IPv6).
    Aims to be compliant with RFC 4861 (Neighbor discovery) and RFC 4862
    (Address autoconfiguration)
  * DHCP, AutoIP/APIPA (Zeroconf) and (stateless) DHCPv6
  * UDP (User Datagram Protocol) including experimental UDP-lite extensions
  * TCP (Transmission Control Protocol) with congestion control, RTT estimation
    fast recovery/fast retransmit and sending SACKs
  * raw/native API for enhanced performance
  * Optional Berkeley-like socket API
  * TLS: optional layered TCP ("altcp") for nearly transparent TLS for any
    TCP-based protocol (ported to mbedTLS) (see changelog for more info)
  * PPPoS and PPPoE (Point-to-point protocol over Serial/Ethernet)
  * DNS (Domain name resolver incl. mDNS)
  * 6LoWPAN (via IEEE 802.15.4, BLE or ZEP)


APPLICATIONS

  * HTTP server with SSI and CGI (HTTPS via altcp)
  * SNMPv2c agent with MIB compiler (Simple Network Management Protocol), v3 via altcp
  * SNTP (Simple network time protocol)
  * NetBIOS name service responder
  * MDNS (Multicast DNS) responder
  * iPerf server implementation
  * MQTT client (TLS support via altcp)


LICENSE

lwIP is freely available under a BSD license.


DEVELOPMENT

lwIP has grown into an excellent TCP/IP stack for embedded devices,
and developers using the stack often submit bug fixes, improvements,
and additions to the stack to further increase its usefulness.

Development of lwIP is hosted on Savannah, a central point for
software development, maintenance and distribution. Everyone can
help improve lwIP by use of Savannah's interface, Git and the
mailing list. A core team of developers will commit changes to the
Git source tree.

The lwIP TCP/IP stack is maintained in the 'lwip' Git module and
contributions (such as platform ports) are in the 'contrib' Git module.

See doc/savannah.txt for details on Git server access for users and
developers.

The current Git trees are web-browsable:
  http://git.savannah.gnu.org/cgit/lwip.git
  http://git.savannah.gnu.org/cgit/lwip/lwip-contrib.git

Submit patches and bugs via the lwIP project page:
  http://savannah.nongnu.org/projects/lwip/

Continuous integration builds (GCC, clang):
  https://travis-ci.org/yarrick/lwip-merged


DOCUMENTATION

Self documentation of the source code is regularly extracted from the current
Git sources and is available from this web page:
  http://www.nongnu.org/lwip/

There is now a constantly growing wiki about lwIP at
  http://lwip.wikia.com/wiki/LwIP_Wiki

Also, there are mailing lists you can subscribe at
  http://savannah.nongnu.org/mail/?group=lwip
plus searchable archives:
  http://lists.nongnu.org/archive/html/lwip-users/
  http://lists.nongnu.org/archive/html/lwip-devel/

lwIP was originally written by Adam Dunkels:
  http://dunkels.com/adam/

Reading Adam's papers, the files in docs/, browsing the source code
documentation and browsing the mailing list archives is a good way to
become familiar with the design of lwIP.

Adam Dunkels <adam@sics.se>
Leon Woestenberg <leon.woestenberg@gmx.net>

```
/*
 * Copyright (c) 2001, 2002 Swedish Institute of Computer Science.
 * All rights reserved. 
 * 
 * Redistribution and use in source and binary forms, with or without modification, 
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED 
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT 
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN 
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING 
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * This file is part of the lwIP TCP/IP stack.
 * 
 * Author: Adam Dunkels <adam@sics.se>
 * /
```

### IDA-LWIP:

```
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
```
