#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#include <sys/time.h>
#include <sys/sysinfo.h>
#include <errno.h>
#include "syslog.h"
#include "types.h"

#define NUM_THREADS 4

#define ASSIGNMENT 1
#define COURSE 2
#define NAME "seqgen"

#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_SEC (1000000000)
#define NUM_CPU_CORES (1)
#define TRUE (DEF_TRUE)
#define FALSE (DEF_FALSE)

int abortTest = FALSE;
int abortS1 = FALSE, abortS2 = FALSE, abortS3 = FALSE;
sem_t semS1, semS2, semS3;
struct timeval start_time_val;

typedef struct
{
    int threadIdx;
    unsigned long long sequencePeriods;
} threadParams_t;

typedef void* (*threadFunc_t)(void*);

void* Sequencer(void* threadp);

void* Service_1(void* threadp);
void* Service_2(void* threadp);
void* Service_3(void* threadp);

double getTimeMsec(void);
void print_scheduler(void);


void main(void)
{
    struct timeval current_time_val;
    int i, rc, scope;
    cpu_set_t threadcpu;
    pthread_t threads[NUM_THREADS];
    threadParams_t threadParams[NUM_THREADS];
    pthread_attr_t rt_sched_attr[NUM_THREADS];
    int rt_max_prio, rt_min_prio;
    struct sched_param rt_param[NUM_THREADS];
    struct sched_param main_param;
    pthread_attr_t main_attr;
    pid_t mainpid;
    cpu_set_t allcpuset;
    threadFunc_t threadFuncs[4] = { Sequencer, Service_1, Service_2, Service_3 };

    syslog_init(NAME, COURSE, ASSIGNMENT);

    syslog_printheader();

    printf("Starting Sequencer Demo\n");
    gettimeofday(&start_time_val, (struct timezone*)0);
    gettimeofday(&current_time_val, (struct timezone*)0);
    SYSLOG_TRACE("Sequencer @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec - start_time_val.tv_sec), (int)current_time_val.tv_usec / USEC_PER_MSEC);

    printf("System has %d processors configured and %d available.\n", get_nprocs_conf(), get_nprocs());

    CPU_ZERO(&allcpuset);

    for (i = 0; i < NUM_CPU_CORES; i++)
        CPU_SET(i, &allcpuset);

    // initialize the sequencer semaphores
    //
    if (sem_init(&semS1, 0, 0)) { printf("Failed to initialize S1 semaphore\n"); exit(-1); }
    if (sem_init(&semS2, 0, 0)) { printf("Failed to initialize S2 semaphore\n"); exit(-1); }
    if (sem_init(&semS3, 0, 0)) { printf("Failed to initialize S3 semaphore\n"); exit(-1); }

    mainpid = getpid();

    rt_max_prio = sched_get_priority_max(SCHED_FIFO);
    rt_min_prio = sched_get_priority_min(SCHED_FIFO);

    rc = sched_getparam(mainpid, &main_param);
    main_param.sched_priority = rt_max_prio;
    rc = sched_setscheduler(getpid(), SCHED_FIFO, &main_param);
    if (rc < 0) perror("main_param");
    print_scheduler();


    pthread_attr_getscope(&main_attr, &scope);

    if (scope == PTHREAD_SCOPE_SYSTEM)
        printf("PTHREAD SCOPE SYSTEM\n");
    else if (scope == PTHREAD_SCOPE_PROCESS)
        printf("PTHREAD SCOPE PROCESS\n");
    else
        printf("PTHREAD SCOPE UNKNOWN\n");

    printf("rt_max_prio=%d\n", rt_max_prio);
    printf("rt_min_prio=%d\n", rt_min_prio);

    threadParams[0].sequencePeriods = 900;

    for (i = 0; i < NUM_THREADS; i++)
    {

        CPU_ZERO(&threadcpu);
        CPU_SET(0, &threadcpu);

        rc = pthread_attr_init(&rt_sched_attr[i]);
        rc = pthread_attr_setinheritsched(&rt_sched_attr[i], PTHREAD_EXPLICIT_SCHED);
        rc = pthread_attr_setschedpolicy(&rt_sched_attr[i], SCHED_FIFO);
        rc = pthread_attr_setaffinity_np(&rt_sched_attr[i], sizeof(cpu_set_t), &threadcpu);

        rt_param[i].sched_priority = rt_max_prio - i;
        pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

        threadParams[i].threadIdx = i;

        rt_param[i].sched_priority = rt_max_prio - i;
        pthread_attr_setschedparam(&rt_sched_attr[i], &rt_param[i]);

        rc = pthread_create(&threads[i], &rt_sched_attr[i], threadFuncs[i], (void*)&threadParams[i]);

        if (rc < 0)
            SYSLOG_TRACE("Error: pthread_create for thread %u", i);

    }

    printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu));

    for (i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("\nTEST COMPLETE\n");
}


void* Sequencer(void* threadp)
{
    struct timeval current_time_val;
    struct timespec delay_time = { 0,33333333 }; // delay for 33.33 msec, 30 Hz
    struct timespec remaining_time;
    double current_time;
    double residual;
    int rc, delay_cnt = 0;
    unsigned long long seqCnt = 0;
    threadParams_t* threadParams = (threadParams_t*)threadp;

    gettimeofday(&current_time_val, (struct timezone*)0);
    SYSLOG_TRACE("Sequencer thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec - start_time_val.tv_sec), (int)current_time_val.tv_usec / USEC_PER_MSEC);
    printf("Sequencer thread @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec - start_time_val.tv_sec), (int)current_time_val.tv_usec / USEC_PER_MSEC);

    do
    {
        delay_cnt = 0; residual = 0.0;

        do
        {
            rc = nanosleep(&delay_time, &remaining_time);

            if (rc == EINTR)
            {
                residual = remaining_time.tv_sec + ((double)remaining_time.tv_nsec / (double)NANOSEC_PER_SEC);

                if (residual > 0.0) printf("residual=%lf, sec=%d, nsec=%d\n", residual, (int)remaining_time.tv_sec, (int)remaining_time.tv_nsec);

                delay_cnt++;
            }
            else if (rc < 0)
            {
                perror("Sequencer nanosleep");
                exit(-1);
            }

        } while ((residual > 0.0) && (delay_cnt < 100));

        gettimeofday(&current_time_val, (struct timezone*)0);

        if (delay_cnt > 1) printf("Sequencer looping delay %d\n", delay_cnt);


        // Release each service at a sub-rate of the generic sequencer rate

        // Servcie_1 = RT_MAX-1	@ 0.5 Hz
        if ((seqCnt % 60) == 0) sem_post(&semS1);

        // Service_2 = RT_MAX-2	@ 0.1 Hz
        if ((seqCnt % 300) == 0) sem_post(&semS2);

        // Service_3 = RT_MAX-3	@ 0.0667 Hz
        if ((seqCnt % 500) == 0) sem_post(&semS3);

        seqCnt++;

    } while (!abortTest && (seqCnt < threadParams->sequencePeriods));

    abortS1 = TRUE; abortS2 = TRUE; abortS3 = TRUE;
    sem_post(&semS1); sem_post(&semS2); sem_post(&semS3);

    pthread_exit((void*)0);
}



void* Service_1(void* threadp)
{
    struct timeval current_time_val;
    double current_time;
    unsigned long long S1Cnt = 0;
    threadParams_t* threadParams = (threadParams_t*)threadp;

    while (!abortS1)
    {
        sem_wait(&semS1);
        if (abortS1)
            break;

        S1Cnt++;

        gettimeofday(&current_time_val, (struct timezone*)0);
        SYSLOG_TRACE("Thread 1 start %llu @ sec=%d, msec=%d on core 0", S1Cnt, (int)(current_time_val.tv_sec - start_time_val.tv_sec), (int)current_time_val.tv_usec / USEC_PER_MSEC);
    }

    pthread_exit((void*)0);
}


void* Service_2(void* threadp)
{
    struct timeval current_time_val;
    double current_time;
    unsigned long long S2Cnt = 0;
    threadParams_t* threadParams = (threadParams_t*)threadp;

    while (!abortS2)
    {
        sem_wait(&semS2);
        if (abortS2)
            break;
        S2Cnt++;

        gettimeofday(&current_time_val, (struct timezone*)0);
        SYSLOG_TRACE("Thread 2 start %llu @ sec=%d, msec=%d on core 0", S2Cnt, (int)(current_time_val.tv_sec - start_time_val.tv_sec), (int)current_time_val.tv_usec / USEC_PER_MSEC);
    }

    pthread_exit((void*)0);
}

void* Service_3(void* threadp)
{
    struct timeval current_time_val;
    double current_time;
    unsigned long long S3Cnt = 0;
    threadParams_t* threadParams = (threadParams_t*)threadp;

    while (!abortS3)
    {
        sem_wait(&semS3);
        if (abortS3)
            break;

        S3Cnt++;

        gettimeofday(&current_time_val, (struct timezone*)0);
        SYSLOG_TRACE("Thread 3 start %llu @ sec=%d, msec=%d on core 0", S3Cnt, (int)(current_time_val.tv_sec - start_time_val.tv_sec), (int)current_time_val.tv_usec / USEC_PER_MSEC);
    }

    pthread_exit((void*)0);
}

double getTimeMsec(void)
{
    struct timespec event_ts = { 0, 0 };

    clock_gettime(CLOCK_MONOTONIC, &event_ts);
    return ((event_ts.tv_sec) * 1000.0) + ((event_ts.tv_nsec) / 1000000.0);
}


void print_scheduler(void)
{
    int schedType;

    schedType = sched_getscheduler(getpid());

    switch (schedType)
    {
    case SCHED_FIFO:
        printf("Pthread Policy is SCHED_FIFO\n");
        break;
    case SCHED_OTHER:
        printf("Pthread Policy is SCHED_OTHER\n"); exit(-1);
        break;
    case SCHED_RR:
        printf("Pthread Policy is SCHED_RR\n"); exit(-1);
        break;
    default:
        printf("Pthread Policy is UNKNOWN\n"); exit(-1);
    }
}
