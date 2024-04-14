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
#define LOG_AND_PRINT

#if defined(DEBUG)
    #if defined(LOG_AND_PRINT)
        #define SYS_TRACE(...) syslog_trace(__VA_ARGS__);  syslog_print(__VA_ARGS__)
    #else
        #define SYS_TRACE(...)  syslog_trace(__VA_ARGS__)
    #endif
#else
#define SYS_TRACE(...)
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



#endif //SYS_LOG_H_