/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_log.h>
#include <stdlib.h>
#include <string.h>

#include <device.h>
#include <esp_matter.h>
#include <led_driver.h>

#include <app_priv.h>

#include "public.h"

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";
// extern uint16_t light_endpoint_id;

static gpio_num_t get_led_gpio(uint16_t endpoint_id)
{
    if (endpoint_id == light_endpoint_id1)
        return LED1;
    if (endpoint_id == light_endpoint_id2)
        return LED2;
    if (endpoint_id == light_endpoint_id3)
        return LED3;
    if (endpoint_id == light_endpoint_id4)
        return LED4;
    return LED1; // Default fallback
}

/* Do any conversions/remapping for the actual value here */
static esp_err_t app_driver_light_set_on_off(esp_matter_attr_val_t *val, uint16_t endpoint_id)
{
    ESP_LOGI(TAG, "Changing the GPIO LED for endpoint %d to %d!", endpoint_id, val->val.b);

    gpio_num_t led_pin = get_led_gpio(endpoint_id);
    gpio_set_level(led_pin, val->val.b);
    ESP_LOGI(TAG, "C........................log2................");

    // Cập nhật trạng thái switch_states tương ứng
    int8_t switch_index;
    ESP_LOGI(TAG, "C........................log3................");

    if (endpoint_id == light_endpoint_id1)
        switch_index = 0;
    else if (endpoint_id == light_endpoint_id2)
        switch_index = 1;
    else if (endpoint_id == light_endpoint_id3)
        switch_index = 2;
    else if (endpoint_id == light_endpoint_id4)
        switch_index = 3;
    else
    {
        return ESP_OK;
    }

    switch_states[switch_index].state = val->val.b;
    ESP_LOGI(TAG, "C........................log1................");
    // }

    return ESP_OK;
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle, uint16_t endpoint_id, uint32_t cluster_id,
                                      uint32_t attribute_id, esp_matter_attr_val_t *val)
{
    esp_err_t err = ESP_OK;

    if (endpoint_id == light_endpoint_id1 ||
        endpoint_id == light_endpoint_id2 ||
        endpoint_id == light_endpoint_id3 ||
        endpoint_id == light_endpoint_id4)
    {
        if (cluster_id == OnOff::Id)
        {
            if (attribute_id == OnOff::Attributes::OnOff::Id)
            {
                err = app_driver_light_set_on_off(val, endpoint_id);
            }
        }
    }

    return err;
}
app_driver_handle_t app_driver_light_init()
{
    // Khởi tạo tất cả LED
    const gpio_num_t leds[] = {LED1, LED2, LED3, LED4};

    for (int i = 0; i < 4; i++)
    {
        esp_rom_gpio_pad_select_gpio(leds[i]);
        gpio_set_direction(leds[i], GPIO_MODE_OUTPUT);
        gpio_set_level(leds[i], 0);
    }

    led_driver_config_t config = led_driver_get_config();
    led_driver_handle_t handle = led_driver_init(&config);
    return (app_driver_handle_t)handle;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    esp_err_t err = ESP_OK;
    node_t *node = node::get();
    endpoint_t *endpoint = endpoint::get(node, endpoint_id);
    cluster_t *cluster = NULL;
    attribute_t *attribute = NULL;
    esp_matter_attr_val_t val = esp_matter_invalid(NULL);

    /* Setting power */
    cluster = cluster::get(endpoint, OnOff::Id);
    attribute = attribute::get(cluster, OnOff::Attributes::OnOff::Id);
    attribute::get_val(attribute, &val);
    err |= app_driver_light_set_on_off(&val, endpoint_id);

    return err;
}
