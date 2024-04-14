#ifndef CAMERA_H_
#define CAMERA_H_

#include "types.h"

void camera_init(INT32 *fd);
void *camera_task(void *threadp);

#endif