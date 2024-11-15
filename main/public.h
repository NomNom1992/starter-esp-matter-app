#ifndef PUBLIC_H
#define PUBLIC_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#define MAX_SWITCH 4
#define ENABLE_TOPIC_2 1
#define ENABLE_TOPIC_3 0
#define ENABLE_TOPIC_4 0

typedef struct
{
    bool state;
    bool is_pressed;            // Trạng thái hiện tại
    uint32_t press_start_time;  // Thời điểm bắt đầu nhấn
    bool long_press_triggered;  // Đã trigger long press chưa
    bool short_press_triggered; // Đã trigger short press chưa
} key_state_t;

extern SemaphoreHandle_t mutex;
extern key_state_t switch_states[MAX_SWITCH];
extern bool saved_mode;
extern char *json_string;
extern uint16_t light_endpoint_id1;
extern uint16_t light_endpoint_id2;
extern uint16_t light_endpoint_id3;
extern uint16_t light_endpoint_id4;
#endif // PUBLIC_H