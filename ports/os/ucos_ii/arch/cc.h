/*
 * cc.h
 *
 *  Created on: 06.12.2018
 *      Author: Kai-Björn Gemlau
 */

#ifndef CC_H_
#define CC_H_

#include <sys/time.h>

#ifndef BYTE_ORDER
#define BYTE_ORDER  LITTLE_ENDIAN
#endif

/**
 * Randon number
 * returns 4, choosen by fair dice role
 */
#define LWIP_RAND() ((u32_t)4)

#define LWIP_PLATFORM_ASSERT(x) do {                \
    } while (0)

#endif /* CC_H_ */
