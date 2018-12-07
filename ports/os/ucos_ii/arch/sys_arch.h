/*
 * sys_arch.h
 *
 *  Created on: 06.12.2018
 *      Author: Kai-Björn Gemlau
 */

#ifndef SYS_ARCH_H_
#define SYS_ARCH_H_

#include "stdint.h"

#define SYS_MBOX_NULL   NULL
#define SYS_SEM_NULL    NULL

typedef void * sys_sem_t;
typedef void * sys_mbox_t;
typedef uint8_t sys_thread_t;

typedef uint8_t sys_prot_t;
sys_prot_t sys_arch_protect(void);
void sys_arch_unprotect(sys_prot_t pval);

#endif /* SYS_ARCH_H_ */
