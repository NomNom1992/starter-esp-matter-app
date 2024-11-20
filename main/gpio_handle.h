#ifdef __cplusplus
extern "C"
{
#endif

#ifndef GPIO_HANDLE_H
#define GPIO_HANDLE_H

#include "esp_err.h"
#include <driver/gpio.h>
#define LED_CONTROL_GPIO ((gpio_num_t)3)  // GPIO để điều khiển đèn
#define MODE_CONTROL_GPIO ((gpio_num_t)5) // GPIO để đổi mode

    esp_err_t gpio_control_init(void);
    void start_gpio_control_task(void);
    void control_gpio_led(int button_index, bool state);
#endif // GPIO_HANDLE_H

#ifdef __cplusplus
}
#endif