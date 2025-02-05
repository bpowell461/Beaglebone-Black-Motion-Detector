/**
 * @file ringbuffer.h
 * @author Brad Powell
 * @date 08 April 2024
 * @brief Statically allocated generic ring buffer macros adapted from Phillip Thrasher's C Generic Ring Buffer.
 *  https://github.com/pthrasher/c-generic-ring-buffer/tree/master
 *
 **/
#ifndef RING_BUFFER_H_
#define RING_BUFFER_H_

#include "types.h"
#include <string.h>

#define ringbuffer_memcpy   memcpy

#define ringbuffer_typedef(TYPE, NAME) \
    typedef struct { \
        TYPE        *data;\
        uint32_t      size; \
        uint32_t      readPtr; \
        uint32_t      writePtr; \
    } NAME

typedef enum
{
    BUFSTATE_EMPTY,
    BUFSTATE_FULL,
    BUFSTATE_COUNT
}buffer_state_e;

#define ringbuffer_init(BUF, TYPE, SIZE) \
    do {\
    { \
        static TYPE buffer[SIZE];\
        BUF.data = buffer; \
    } \
    BUF.size = SIZE; \
    BUF.writePtr = 0; \
    BUF.readPtr = 0; \
    } while(0)

#define ringbuffer_inc_writeptr(BUF)     ((BUF)->writePtr = ((BUF)->writePtr + 1u) & ((BUF)->size - 1u))
#define ringbuffer_write(BUF, DATA) \
    do { \
        (BUF)->data[(BUF)->writePtr] = DATA; \
        ringbuffer_inc_writeptr((BUF)); \
    }while(0) 
#define ringbuffer_write_memcpy(BUF, DATA, SIZE) \
    do { \
        ringbuffer_memcpy(&((BUF)->data[(BUF)->writePtr]), (DATA), (SIZE)); \
        ringbuffer_inc_writeptr((BUF)); \
    }while(0) 

#define ringbuffer_inc_readptr(BUF)     ((BUF)->readPtr = ((BUF)->readPtr + 1u) & ((BUF)->size - 1u))
#define ringbuffer_read(BUF, DATA) \
    do { \
        DATA = (BUF)->data[(BUF)->readPtr]; \
        ringbuffer_inc_readptr((BUF)); \
    }while(0)

#define ringbuffer_read_zc(BUF, DATA)   (DATA = &((BUF)->data[(BUF)->readPtr]))

#define ringbuffer_getlen(BUF)  (((BUF)->writePtr - (BUF)->readPtr) & ((BUF)->size - 1u))
#define ringbuffer_isEmpty(BUF) (((BUF)->readPtr) == ((BUF)->writePtr))
#define ringbuffer_isFull(BUF)  ((ringbuffer_getlen((BUF))) == ((BUF)->size - 1u))

#endif /* RING_BUFFER_H_ */