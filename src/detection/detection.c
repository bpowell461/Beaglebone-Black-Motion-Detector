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

static osal_mqueue_t mqueue;

static uint8_t motion_detected = false;
static uint8_t exit_flag = false;

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
    while (true) 
    {
        if (exit_flag) 
        {
            break;
        }
        uint8_t cur_status = read_gpio_value();

        if (cur_status && !motion_detected) 
        {
            motion_detected = true;
            event_e event = EVENT_MOTION_DETECTED;
            osal_queue_send(&mqueue, &event, sizeof(event_e));
            SYS_TRACE("Motion Detected: %d", motion_detected);
        } 
        else if (!cur_status && motion_detected) 
        {
            nanosleep((const struct timespec[]){{5, 0L}}, NULL); // Sleep for 5 seconds
            if (!read_gpio_value()) 
            {
                motion_detected = false;
                event_e event = EVENT_MOTION_LOST;
                osal_queue_send(&mqueue, &event, sizeof(event_e));
                SYS_TRACE("Motion Lost: %d", motion_detected);
            }
        }

        osal_task_delay(id);
    }

    osal_task_delete(id, false);

    return NULL;
}

void *detection_exit(void *threadp)
{
    exit_flag = true;
    deinitialize_gpio();
    return NULL;
}