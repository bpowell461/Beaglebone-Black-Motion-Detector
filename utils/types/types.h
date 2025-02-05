/**
 * @file types.h
 * @author Brad Powell
 * @date 16 Mar 2024
 * @brief File containing common types and macros.
 *
 */
#ifndef TYPES_H_
#define TYPES_H_

#include <stdint.h>
#include <stddef.h>

#define true        (1u == 1u)
#define false       (!true)

typedef enum
{
    SYS_SUCCESS,
    SYS_FAILURE,
    SYS_IGNORE,
    SYS_COUNT
}sys_result_e;

#endif // !TYPES_H_
