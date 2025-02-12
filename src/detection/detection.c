#include "detection.h"
#include "osal.h"
#include "syslog.h"
#include "events.h"

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>

#define GPIO_PIN "60" // Replace with your actual GPIO pin number
#define GPIO_PATH "/sys/class/gpio/gpio" GPIO_PIN "/value"
#define STARTUP_DELAY 10 // 10 seconds startup delay

typedef enum {
    eSTATE_DETECTING,
    eSTATE_MOTION_DETECTED,
    eSTATE_MOTION_LOST,
    eSTATE_TIMEOUT,
    eSTATE_EXITING,
    eSTATE_EXIT
}detection_fsm_state_e;

typedef enum {
    eSUBSTATE_ENTRY,
    eSUBSTATE_RUN,
    eSUBSTATE_EXIT
}detection_fsm_substate_e;

static osal_mqueue_t mqueue;

static uint8_t motion_detected = false;
static detection_fsm_state_e detection_state = eSTATE_DETECTING;
static detection_fsm_substate_e detection_substate = eSUBSTATE_ENTRY;
static struct timespec timeout_start;

static void set_detection_state(detection_fsm_state_e state);
static void process_next_state(uint8_t cur_status);

int initialize_gpio() {
    int fd;
    char buf[64];

    // Export the GPIO pin
    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) {
        perror("Failed to open gpio export for writing");
        return -1;
    }

    snprintf(buf, sizeof(buf), "%s\n", GPIO_PIN);

    if (write(fd, buf, strlen(buf)) < 0) {
        perror("Failed to write to gpio export");
        close(fd);
        return -1;
    }

    close(fd);

    // Set the GPIO pin direction to input
    snprintf(buf, sizeof(buf), "/sys/class/gpio/gpio%s/direction", GPIO_PIN);
    fd = open(buf, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open gpio direction for writing");
        return -1;
    }

    if (write(fd, "in\n", 3) < 0) {
        perror("Failed to write to gpio direction");
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}

int deinitialize_gpio()
{
    int fd;
    char buf[64];

    // Unexport the GPIO pin
    fd = open("/sys/class/gpio/unexport", O_WRONLY);
    if (fd < 0) {
        perror("Failed to open gpio unexport for writing");
        return -1;
    }

    snprintf(buf, sizeof(buf), "%s\n", GPIO_PIN);

    if (write(fd, buf, strlen(buf)) < 0) {
        perror("Failed to write to gpio unexport");
        close(fd);
        return -1;
    }

    close(fd);

    return 0;
}

int read_gpio_value() {
    int fd;
    char value;

    fd = open(GPIO_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open gpio value for reading");
        return -1;
    }

    if (read(fd, &value, 1) != 1) {
        perror("Failed to read value");
        close(fd);
        return -1;
    }

    close(fd);
    return (value == '1') ? 1 : 0;
}

void detection_init(void)
{
    initialize_gpio();
    osal_queue_create(&mqueue, EVENT_QUEUE_NAME, 10, sizeof(event_e));
}

void *detection_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    SYS_TRACE("Detection Task (ID: %u) Starting...", id);

    // Main loop to read the GPIO value
    while (detection_state != eSTATE_EXIT) 
    {
        uint8_t cur_status = read_gpio_value();
        process_next_state(cur_status);
        
        switch(detection_state)
        {
            case eSTATE_DETECTING:
            {
                // This is our "idle" state, there isn't much to do here for now
                break;
            }
            case eSTATE_MOTION_DETECTED:
            {
                motion_detected = true;
                event_e event = EVENT_MOTION_DETECTED;
                osal_queue_send(&mqueue, &event, sizeof(event_e));
                SYS_TRACE("Motion Detected: %d", motion_detected);
                set_detection_state(eSTATE_DETECTING);
                break;
            }
            case eSTATE_MOTION_LOST:
            {
                motion_detected = false;
                event_e event = EVENT_MOTION_LOST;
                osal_queue_send(&mqueue, &event, sizeof(event_e));
                SYS_TRACE("Motion Lost: %d", motion_detected);
                set_detection_state(eSTATE_DETECTING);
                break;
            }
            case eSTATE_TIMEOUT:
            {
                switch(detection_substate)
                {
                    case eSUBSTATE_ENTRY:
                    {
                        detection_substate = eSUBSTATE_RUN;
                        clock_gettime(CLOCK_MONOTONIC, &timeout_start);
                        SYS_TRACE("Starting Timeout...");
                        break;
                    }
                    case eSUBSTATE_RUN:
                    {
                        struct timespec current;
                        clock_gettime(CLOCK_MONOTONIC, &current);
                        double elapsed = (current.tv_sec - timeout_start.tv_sec) + 
                                        (current.tv_nsec - timeout_start.tv_nsec) / 1e9;

                        if (elapsed >= 5.0) 
                        {
                            detection_substate = eSUBSTATE_EXIT;
                            break;
                        }
                        
                        break;
                    }
                    case eSUBSTATE_EXIT:
                    {
                        motion_detected = false;
                        set_detection_state(eSTATE_MOTION_LOST);
                        break;
                    }
                }
                break;
            }
            default:
            {
                break;
            }
        }

        osal_task_delay(id);
    }

    osal_task_delete(id, false);

    return NULL;
}

void *detection_exit(void *threadp)
{
    set_detection_state(eSTATE_EXIT);
    deinitialize_gpio();
    return NULL;
}

static void set_detection_state(detection_fsm_state_e state)
{
    SYS_TRACE("Detection State: %d", state);
    detection_state = state;
}

static void process_next_state(uint8_t cur_status)
{
    if (cur_status && !motion_detected) 
    {
        if (detection_state == eSTATE_TIMEOUT)
        {
            SYS_TRACE("Timeout Cancelled...");
        }
        set_detection_state(eSTATE_MOTION_DETECTED);
    }
    else if (!cur_status && motion_detected) 
    {
        // Set the state to timeout and sub-state for entry'
        motion_detected = false;
        if (detection_state != eSTATE_TIMEOUT)
        {
            detection_substate = eSUBSTATE_ENTRY;
            set_detection_state(eSTATE_TIMEOUT);
        }
    }
}