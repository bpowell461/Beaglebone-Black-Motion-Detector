#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <semaphore.h>

#include <syslog.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <errno.h>

#include <signal.h>
#include "syslog.h"
#include "types.h"

#define NUM_THREADS 3

#define ASSIGNMENT 2
#define COURSE 2
#define NAME "seqgen"

#define USEC_PER_MSEC (1000)
#define NANOSEC_PER_SEC (1000000000)
#define NUM_CPU_CORES (1)
#define TRUE (DEF_TRUE)
#define FALSE (DEF_FALSE)

#define TIMER_HZ (1)
#define TIMER_PERIOD (30)

#define S1_SEC (2)
#define S2_SEC (5)
#define S3_SEC (15)

#define S1_CAPACITY (1)
#define S2_CAPACITY (1)
#define S3_CAPACITY (2)

int abortTest = FALSE;
int abortS1 = FALSE, abortS2 = FALSE, abortS3 = FALSE;
sem_t semS1, semS2, semS3;
struct timeval start_time_val;

static unsigned int S1_Ctr = 0;
static unsigned int S2_Ctr = 0;
static unsigned int S3_Ctr = 0;

static timer_t PIT;
static struct itimerspec itime = { {1,0}, {1,0} };
static struct itimerspec last_itime;

static unsigned long long seqCnt = 0;

typedef struct
{
    int threadIdx;
    unsigned long long sequencePeriods;
} threadParams_t;

typedef void* (*threadFunc_t)(void*);

void DoWork(int capacity);

void Sequencer(int id);

void* Service_1(void* threadp);
void* Service_2(void* threadp);
void* Service_3(void* threadp);

double getTimeMsec(void);
void print_scheduler(void);


void main(void)
{
    struct timeval current_time_val;
    int rc, scope, flags = 0;
    cpu_set_t threadcpu;
    pthread_t threads[NUM_THREADS];
    threadParams_t threadParams[NUM_THREADS];
    pthread_attr_t rt_sched_attr[NUM_THREADS];
    unsigned int rt_max_prio, rt_min_prio;
    struct sched_param rt_param[NUM_THREADS];
    struct sched_param main_param;
    pthread_attr_t main_attr;
    pid_t mainpid;
    cpu_set_t allcpuset;
    threadFunc_t threadFuncs[NUM_THREADS] = { Service_1, Service_2, Service_3 };

    syslog_init(NAME, COURSE, ASSIGNMENT);

    syslog_printheader();

    printf("Starting Sequencer Demo\n");
    gettimeofday(&start_time_val, (struct timezone*)0);
    gettimeofday(&current_time_val, (struct timezone*)0);
    SYSLOG_TRACE("Sequencer @ sec=%d, msec=%d\n", (int)(current_time_val.tv_sec - start_time_val.tv_sec), (int)current_time_val.tv_usec / USEC_PER_MSEC);

    printf("System has %d processors configured and %d available.\n", get_nprocs_conf(), get_nprocs());

    CPU_ZERO(&allcpuset);

    for (unsigned int i = 0; i < NUM_CPU_CORES; i++)
        CPU_SET(i, &allcpuset);

    // initialize the sequencer semaphores
    //
    if (sem_init(&semS1, 0, 0)) { printf("Failed to initialize S1 semaphore\n"); exit(-1); }
    if (sem_init(&semS2, 0, 0)) { printf("Failed to initialize S2 semaphore\n"); exit(-1); }
    if (sem_init(&semS3, 0, 0)) { printf("Failed to initialize S3 semaphore\n"); exit(-1); }

    /* Dynamic calculation of counter values of Task Periods for a Timer */
    S1_Ctr = (unsigned int) floor(((double)TIMER_HZ) / (1 / (double)S1_SEC));
    S2_Ctr = (unsigned int) floor(((double)TIMER_HZ) / (1 / (double)S2_SEC));
    S3_Ctr = (unsigned int) floor(((double)TIMER_HZ) / (1 / (double)S3_SEC));

    printf("S1 Ctr: %d \n", S1_Ctr);
    printf("S1 Ctr: %d \n", S2_Ctr);
    printf("S1 Ctr: %d \n", S3_Ctr);

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

    threadParams[0].sequencePeriods = S1_CAPACITY;
    threadParams[1].sequencePeriods = S2_CAPACITY;
    threadParams[2].sequencePeriods = S3_CAPACITY;

    for (unsigned int i = 0; i < NUM_THREADS; i++)
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

    timer_create(CLOCK_REALTIME, NULL, &PIT);

    signal(SIGALRM, (void(*)()) Sequencer);

    /* 1000ms Periodic Interval Timer (PIT) */
    itime.it_interval.tv_sec = 1;
    itime.it_interval.tv_nsec = 0;
    itime.it_value.tv_sec = 1;
    itime.it_value.tv_nsec = 0;

    timer_settime(PIT, flags, &itime, &last_itime);

    printf("Service threads will run on %d CPU cores\n", CPU_COUNT(&threadcpu));

    for (unsigned int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("\nTEST COMPLETE\n");
}


void Sequencer(int id)
{
    int flags = 0;

    /* Stop Sequencing Timer */
    if (abortTest || seqCnt >= TIMER_PERIOD)
    {
        itime.it_interval.tv_sec = 0;
        itime.it_interval.tv_nsec = 0;
        itime.it_value.tv_sec = 0;
        itime.it_value.tv_nsec = 0;
        timer_settime(PIT, flags, &itime, &last_itime);

        abortS1 = TRUE; abortS2 = TRUE; abortS3 = TRUE;
        sem_post(&semS1); sem_post(&semS2); sem_post(&semS3);

        return;
    }

    printf("Sequencer invoked: Count=%llu\n", seqCnt);

    // Servcie_1 = RT_MAX-1	@ 0.5 Hz
    if ((seqCnt % S1_Ctr) == 0) sem_post(&semS1);

    // Service_2 = RT_MAX-2	@ 0.25 Hz
    if ((seqCnt % S2_Ctr) == 0) sem_post(&semS2);

    // Service_3 = RT_MAX-3	@ 0.142857142 Hz
    if ((seqCnt % S3_Ctr) == 0) sem_post(&semS3);

    seqCnt++;

}

void* Service_1(void* threadp)
{
    struct timeval current_time_val;
    unsigned long long S1Cnt = 0;
    threadParams_t* threadParams = (threadParams_t*)threadp;

    while (!abortS1)
    {
        sem_wait(&semS1);
        if (abortS1)
            break;

        S1Cnt++;
        printf("Service 1 Count = %\n", S1Cnt);
        gettimeofday(&current_time_val, (struct timezone*)0);
        SYSLOG_TRACE("Thread 1 start %llu @ sec=%d, msec=%d on core 0", S1Cnt, (int)(current_time_val.tv_sec - start_time_val.tv_sec), (int)current_time_val.tv_usec / USEC_PER_MSEC);
        DoWork(threadParams->sequencePeriods);
    }

    pthread_exit((void*)0);
}


void* Service_2(void* threadp)
{
    struct timeval current_time_val;
    unsigned long long S2Cnt = 0;
    threadParams_t* threadParams = (threadParams_t*)threadp;

    while (!abortS2)
    {
        sem_wait(&semS2);
        if (abortS2)
            break;
        S2Cnt++;
        printf("Service 2 Count = %\n", S2Cnt);
        gettimeofday(&current_time_val, (struct timezone*)0);
        SYSLOG_TRACE("Thread 2 start %llu @ sec=%d, msec=%d on core 0", S2Cnt, (int)(current_time_val.tv_sec - start_time_val.tv_sec), (int)current_time_val.tv_usec / USEC_PER_MSEC);
        DoWork(threadParams->sequencePeriods);
    }

    pthread_exit((void*)0);
}

void* Service_3(void* threadp)
{
    struct timeval current_time_val;
    unsigned long long S3Cnt = 0;
    threadParams_t* threadParams = (threadParams_t*)threadp;

    while (!abortS3)
    {
        sem_wait(&semS3);
        if (abortS3)
            break;

        S3Cnt++;
        printf("Service 3 Count = %\n", S3Cnt);
        gettimeofday(&current_time_val, (struct timezone*)0);

        SYSLOG_TRACE("Thread 3 start %llu @ sec=%d, msec=%d on core 0", S3Cnt, (int)(current_time_val.tv_sec - start_time_val.tv_sec), (int)current_time_val.tv_usec / USEC_PER_MSEC);
        DoWork(threadParams->sequencePeriods);
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

void DoWork(int capacity)
{
    volatile unsigned int workCtr = 0;

    /* "Heavy" thread */
    for (volatile int i = 0; i < capacity; i++)
    {
        while (workCtr < 999999)
        {
            workCtr++;
        }
    }
    
}