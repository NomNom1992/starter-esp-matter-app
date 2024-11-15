#include "gpio_handle.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "public.h"
#include "mqtt_service.h"
#include "nvs_manager.h"
#include <app_priv.h>
#include <app/clusters/on-off-server/on-off-server.h>

using namespace esp_matter;
using namespace chip::app::Clusters;

static const char *TAG = "GPIO_HANDLE";

#define DEBOUNCE_TIME_MS 50

esp_err_t gpio_control_init(void)
{
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << LED_CONTROL_GPIO) | (1ULL << MODE_CONTROL_GPIO);
    io_conf.pull_down_en = GPIO_PULLDOWN_ENABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    return gpio_config(&io_conf);
}
void control_gpio_led(int button_index, bool state)
{
    gpio_num_t led_pin;

    // Map button index to corresponding LED
    switch (button_index)
    {
    case 0:
        led_pin = LED1;
        break;
    case 1:
        led_pin = LED2;
        break;
    case 2:
        led_pin = LED3;
        break;
    case 3:
        led_pin = LED4;
        break;
    default:
        ESP_LOGE(TAG, "Invalid button index");
        return;
    }

    // Set GPIO level
    gpio_set_level(led_pin, state);
    ESP_LOGI(TAG, "LED %d set to %d", button_index + 1, state);
}
static void gpio_control_task(void *pvParameters)
{
    bool last_led_state = false;
    bool last_mode_state = false;
    bool current_led_state;
    bool current_mode_state;

    while (1)
    {
        current_led_state = gpio_get_level(LED_CONTROL_GPIO);
        current_mode_state = gpio_get_level(MODE_CONTROL_GPIO);

        // Xử lý điều khiển đèn
        if (current_led_state != last_led_state)
        {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS)); // Debounce
            current_led_state = gpio_get_level(LED_CONTROL_GPIO);

            if (current_led_state != last_led_state)
            {
                switch_states[0].state = current_led_state;

                if (saved_mode)
                {
                    // MQTT mode
                    control_gpio_led(0, switch_states[0].state);
                    publish_electrode_state(0, switch_states[0].state);
                }
                else
                {
                    // Matter mode
                    esp_matter_attr_val_t val = esp_matter_bool(switch_states[0].state);
                    attribute::update(light_endpoint_id1, OnOff::Id,
                                      OnOff::Attributes::OnOff::Id, &val);
                    app_driver_attribute_update(NULL, light_endpoint_id1,
                                                OnOff::Id,
                                                OnOff::Attributes::OnOff::Id, &val);
                }
                last_led_state = current_led_state;
            }
        }

        // Xử lý đổi mode
        if (current_mode_state != last_mode_state)
        {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME_MS)); // Debounce
            current_mode_state = gpio_get_level(MODE_CONTROL_GPIO);

            if (current_mode_state && !last_mode_state)
            { // Rising edge
                saved_mode = !saved_mode;
                ESP_LOGI(TAG, "Mode changed to: %d", saved_mode);

                esp_err_t err = save_mode_config_to_nvs(&saved_mode);
                if (err == ESP_OK)
                {
                    esp_restart();
                }
            }
            last_mode_state = current_mode_state;
        }

        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void start_gpio_control_task()
{
    xTaskCreate(gpio_control_task, "gpio_control_task", 4096, NULL, 15, NULL);
}