/**
 * @file syslog.h
 * @author Brad Powell
 * @date 16 Mar 2024
 * @brief File containing definitions for system log functions.
 *
 */

#ifndef SYS_LOG_H_
#define SYS_LOG_H_

#include "types.h"
#include <time.h>

#define SYSLOG_MEASURING_ENABLED

#if defined(DEBUG)
    #if defined(LOG_AND_PRINT)
        #define SYS_TRACE(...) do { syslog_trace(__VA_ARGS__);  syslog_print(__VA_ARGS__); }while(0)
    #else
        #define SYS_TRACE(...)  syslog_trace(__VA_ARGS__)
    #endif

    #if defined(SYSLOG_MEASURING_ENABLED)
        #define SYSLOG_INITMEASURE() struct timespec measure[2]
        #define SYSLOG_INITPROFILE() time_t times[2]
        #define SYSLOG_MEASURE(x, y) \
            clock_gettime(CLOCK_REALTIME, &measure[0]); \
            ((x)); \
            clock_gettime(CLOCK_REALTIME, &measure[1]); \
            SYS_TRACE("[SYSLOG_MEASURE] EXECUTION TIME (%s): %llu usec", (y), (measure[1].tv_nsec - measure[0].tv_nsec) / (1000))

        #define SYSLOG_STARTPROFILE(y) \
            time(&times[0]); \
            SYS_TRACE("[SYSLOG_MEASURE] START TIME (%s): %ld sec", (y), times[0]) 

        #define SYSLOG_ENDPROFILE(y) \
             time(&times[1]); \
             SYS_TRACE("[SYSLOG_MEASURE] END TIME (%s): %ld sec", (y), times[1])
            
    #else
        #define SYSLOG_INITMEASURE() {}
        #define SYSLOG_MEASURE(x, y) x
    #endif

#else
#define SYS_TRACE(...) {}
#define SYSLOG_INITMEASURE() {}
#define SYSLOG_MEASURE(x, y) x
#endif

/** 
* Initializes the System Log module.
* @param assignment - assignment name
* @param courseNum - course number
* @param assignmentNum - assignment number
* @return Result of operation.
*/
sys_result_e syslog_init(char *assignment, const int courseNum, const int assignmentNum);

/**
* Closes the System Log module.
* @param  None
* @return Result of operation.
*/
sys_result_e syslog_close(void);

/**
* Prints the System Log Header.
* @param None
* @return None
*/
void syslog_printheader(void);

/**
* Logs a system statement.
* @param msg - string to be printed
* @param varargs - variable amount of args
* @return None
*/
void syslog_trace(const char* msg, ...);

/**
* Prints a system statement.
* @param msg - string to be printed
* @param varargs - variable amount of args
* @return None
*/
void syslog_print(const char *msg, ...);

/**
* Gets the system name.
* @return Character string
*/
const char *syslog_getsysname(void);


#endif //SYS_LOG_H_