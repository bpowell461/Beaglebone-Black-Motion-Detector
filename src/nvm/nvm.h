#ifndef NVM_H_
#define NVM_H_

#include "types.h"

void nvm_init(INT32 *fd);
void *nvm_task(void *threadp);

#endif