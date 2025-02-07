#ifndef DETECTION_H_
#define DETECTION_H_

#include "types.h"

void detection_init(void);
void *detection_task(void *threadp);
void *detection_exit(void *threadp);

#endif
