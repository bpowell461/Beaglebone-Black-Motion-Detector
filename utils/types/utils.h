#ifndef UTILS_H_
#define UTILS_H_

#include <string.h>

#if defined(BEAGLEBONE)
#define PLATFORM "beaglebone"
#define NUM_CPU_CORES (1)
#elif defined(RASPBERRYPI)
#define PLATFORM "raspberrypi"
#define NUM_CPU_CORES (4)
#else
#error "No platform defined!"
#endif

#define MSEC_PER_SEC  (1000u)
#define USEC_PER_MSEC (1000u)
#define NSEC_PER_USEC (1000u)

#define NSEC_PER_SEC  (MSEC_PER_SEC*USEC_PER_MSEC*NSEC_PER_USEC)

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#define CLAMP_UPPER(x, y) ((x) = (x) > (y) ? (y) : (x))
#define CLAMP_LOWER(x, y) ((x) = (x) < (y) ? (y) : (x))

#define IS_ERROR(x)     ((x) < 0 ? SYS_FAILURE : SYS_SUCCESS)

#endif
