#ifndef CAMERA_H_
#define CAMERA_H_

#include "types.h"

void camera_init(int *fd);
void *camera_task(void *threadp);
void *camera_exit(void *threadp);

#endif