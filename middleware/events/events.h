typedef enum
{
    EVENT_NONE = 0,
    EVENT_MOTION_DETECTED,
    EVENT_MOTION_LOST
} event_e;

#define EVENT_QUEUE_NAME "/motion_events"