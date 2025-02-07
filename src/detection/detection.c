#include "detection.h"
#include "osal.h"
#include "syslog.h"

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

void signal_handler(int signo) {
    if (signo == SIGINT) {
        printf("Closing motion detector...\n");
        deinitialize_gpio();
        exit(0);
    }
}

int register_signal_handler() {
    struct sigaction sa;

    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;

    if (sigaction(SIGINT, &sa, NULL) < 0) {
        perror("Failed to register signal handler");
        return -1;
    }

    return 0;
}

void detection_init(void)
{
    register_signal_handler();
    initialize_gpio();
    osal_queue_create(&mqueue, EVENT_QUEUE_NAME, 10, sizeof(event_e));
}

void *detection_task(void *threadp)
{
    osal_task_start_args_t args = *(osal_task_start_args_t *)threadp;

    osal_id_t id = args.task_id;

    SYS_TRACE("Detection Task (ID: %u) Waiting for Start...", id);

    osal_task_wait_start(id);

    // Main loop to read the GPIO value
    struct pollfd fds;
    int timeout_ms = 5000; // 5 seconds timeout in milliseconds

    fds.fd = open(GPIO_PATH, O_RDONLY);
    if (fds.fd < 0) {
        perror("Failed to open gpio value for polling");
        return NULL;
    }
    fds.events = POLLPRI | POLLERR;

    while (true) 
    {
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
            int ret = poll(&fds, 1, timeout_ms);
            if (ret == 0) 
            {
                if (!read_gpio_value()) 
                {
                    motion_detected = false;
                    event_e event = EVENT_MOTION_LOST;
                    osal_queue_send(&mqueue, &event, sizeof(event_e));
                    SYS_TRACE("Motion Lost: %d", motion_detected);
                }
            } 
            else if (ret < 0) 
            {
                perror("Poll error");
            }
        }

        osal_task_delay(id);
    }

    close(fds.fd);

    return NULL;
}

void *detection_exit(void *threadp)
{
    deinitialize_gpio();
    return NULL;
}