/**
 * @file syslog.c
 * @author Brad Powell
 * @date 16 Mar 2024
 * @brief File containing implementation for system log functions.
 *
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <fcntl.h>
#include <unistd.h>
#include "syslog.h"
#include "utils.h"

/* Macros Defines */
#define USE_TRACE 1

#define NULL_TERM_SIZE          (1u)
#define MONTH_SIZE              (3u)
#define DAY_SIZE                (2u)
#define TIME_SIZE               (8u)
#define SYSLOG_FILE_NAME_SIZE   (19u + NULL_TERM_SIZE)

/* Types */
typedef int (*printFunc_t)(const char *, ...);
typedef int (*vprintFunc_t)(const char *, __gnuc_va_list);

/* Function Prototypes */
static int trace_write(const char * msg, ...);
static int vtrace_write(const char * msg, va_list argp);

/* Static Members */
static char         *assignment_name;
static int          course_num;
static int          assignment_num;
static BOOL_T       isInitialized = DEF_FALSE;
static printFunc_t  _printFunc;
static vprintFunc_t _vprintFunc;
static int          trace_fd;
static char         sys_name[256];

/* Function Implementations */

sys_result_e syslog_init(char* assignment, const int courseNum, const int assignmentNum)
{
    if (!isInitialized)
    {
        assignment_name = assignment;
        course_num = courseNum;
        assignment_num = assignmentNum;

        char buf[SYSLOG_FILE_NAME_SIZE];
        snprintf(buf, sizeof(buf), "syslog-prog-%i.%i.txt", courseNum, assignmentNum);

        trace_fd = open(buf, O_WRONLY | O_CREAT | O_TRUNC | O_NONBLOCK);
        if (trace_fd < 0)
            return SYS_FAILURE;

        /* Currently this is very Linux Specific */
        FILE *fp;
        char path[1035];

        /* Open the command for reading. */
        fp = popen("uname -a", "r");
        if (fp == NULL) {
            return SYS_FAILURE;
        }

        /* Read the output a line at a time - output it. */
        if (fgets(path, sizeof(path), fp) != NULL) {
            strncpy(sys_name, path, 256);
        }

        /* close */
        pclose(fp);


        /* Hook for OS or HAL specific trace function */
#if defined(USE_TRACE) && USE_TRACE
        _printFunc = &trace_write;
        _vprintFunc = &vtrace_write;
#else
        _printFunc = DEF_NULL_PTR;
        _vprintFunc = DEF_NULL_PTR;
#endif

        isInitialized = DEF_TRUE;

        return SYS_SUCCESS;
    }

    return SYS_IGNORE;
}

sys_result_e syslog_close(void)
{
    if (isInitialized)
    {
        close(trace_fd);
        _printFunc = DEF_NULL_PTR;
        _vprintFunc = DEF_NULL_PTR;

        isInitialized = DEF_FALSE;

        return SYS_SUCCESS;
    }

    return SYS_IGNORE;
}

void syslog_printheader(void)
{
    if (isInitialized)
    {
        SYS_TRACE(sys_name);
    }

}

void syslog_trace(const char *msg, ...)
{
    if (isInitialized)
    {
        /* Getting date and time in format MM DD HH:MM:SS*/

        time_t t = time(NULL);
        struct tm *local = localtime(&t);
    
        char month[MONTH_SIZE + NULL_TERM_SIZE];
        char day[DAY_SIZE + NULL_TERM_SIZE];
        char ti [TIME_SIZE + NULL_TERM_SIZE];
    
        char date[MONTH_SIZE+DAY_SIZE+TIME_SIZE+NULL_TERM_SIZE + NULL_TERM_SIZE + NULL_TERM_SIZE + NULL_TERM_SIZE];
    
        const char *platform = PLATFORM;
    
        (void) strftime(month, sizeof(month), "%b", local);
        (void) strftime(day, sizeof(day), "%d", local);
        (void) strftime(ti, sizeof(ti), "%X", local);
    
        snprintf(date, sizeof(month) + sizeof(day) + sizeof(ti) + NULL_TERM_SIZE, "%s %s %s", month, day, ti);

        char fmt[] = "%s %s [COURSE:%i][ASSIGNMENT:%i]: ";
        _printFunc(fmt, date, platform, course_num, assignment_num);

        /* Retrieving varargs list for use with printf */
        va_list argp;
        va_start(argp, msg);

        /* For now we are appending a newline character */
        if ('\n' != msg[strlen(msg)])
        {
            char formatted_msg[256];
            snprintf(formatted_msg, sizeof(formatted_msg), "%s\n", msg);
            _vprintFunc(formatted_msg, argp);
        }
        else
        {
            _vprintFunc(msg, argp);
        }
        va_end(argp);

    }
    
}

const char *syslog_getsysname(void)
{
    return sys_name;
}

void syslog_print(const char *msg, ...)
{
    va_list argp;
    va_start(argp, msg);

    /* For now we are appending a newline character */
    if ('\n' != msg[strlen(msg)])
    {
        char formatted_msg[256];
        snprintf(formatted_msg, sizeof(formatted_msg), "%s\n", msg);
        vprintf(formatted_msg, argp);
    }
    else
    {
        vprintf(msg, argp);
    }
    va_end(argp);
}

static int trace_write(const char *msg , ...)
{
    va_list ap;
    char buf[256];
    int n;

    if (trace_fd < 0)
        return 0;  

    va_start(ap, msg);
    n = vsnprintf(buf, 256, msg, ap);
    va_end(ap);

    if(n > -1)
        write(trace_fd, buf, (size_t) n);

    return n;
}

static int vtrace_write(const char *msg, va_list argp)
{
    char buf[256];
    int n;

    if (trace_fd < 0)
        return 0;

    n = vsnprintf(buf, 256, msg, argp);

    if (n > -1)
        write(trace_fd, buf, (size_t) n);

    return n;
}
