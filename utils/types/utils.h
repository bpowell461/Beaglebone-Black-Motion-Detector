#ifndef UTILS_H_
#define UTILS_H_

#if defined(BEAGLEBONE)
#define PLATFORM "beaglebone"
#define NUM_CPU_CORES (1)
#elif defined(RASPBERRYPI)
#define PLATFORM "raspberrypi"
#define NUM_CPU_CORES (4)
#else
#error "No platform defined!"
#endif

#define MSEC_PER_SEC (1000u)
#define USEC_PER_MSEC (1000u)
#define NANOSEC_PER_SEC (1000000000u)

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#endif
