#ifdef __cplusplus
extern "C"
{
#endif

#ifndef MPR121_HANDLE_H
#define MPR121_HANDLE_H
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "freertos/queue.h"
// #include "public.h"
// #include "mpr121.h"
#include "esp_err.h"

    esp_err_t i2c_master_init(void);
    esp_err_t mpr121_init(void);
    void start_touch_event_task();
    void control_gpio_led(int button_index, bool state);
// static void handle_touch_events(MPR121_t *dev, QueueHandle_t queue);
#endif // MPR121_TASK_H

#ifdef __cplusplus
}
#endif