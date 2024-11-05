#pragma GCC diagnostic ignored "-Wnarrowing"
#include <stdio.h>
#include <string.h>
// #include <stdlib.h>

#include "esp_log.h"
// #include "driver/gpio.h"
#include "mpr121_handle.h"
#include "public.h"
#include "nvs_manager.h"
// #include "public.h"
#include "mqtt_service.h"

#include "driver/i2c.h"
#include <app_priv.h>
// #include <app/clusters/on-off-server/on-off-server.h>
// #include <device.h>
using namespace esp_matter;
using namespace chip::app::Clusters;

#define I2C_MASTER_SCL_IO 6       // GPIO cho SCL
#define I2C_MASTER_SDA_IO 5       // GPIO cho SDA
#define I2C_MASTER_NUM I2C_NUM_0  // i2c port number
#define I2C_MASTER_FREQ_HZ 400000 // I2C frequency
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define MPR121_ADDR 0x5A // Địa chỉ I2C của MPR121

#define TOUCH_STATUS_REG 0x00
#define ELE0_TOUCH_THRESHOLD 0x41
#define ELE0_RELEASE_THRESHOLD 0x42

#define LONG_PRESS_TIME_MS 3000  // Thời gian cho long press
#define SHORT_PRESS_TIME_MS 1000 // Thời gian tối đa cho short press

static const char *TAG = "MPR121";

// Hàm khởi tạo I2C master
esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master = {
            .clk_speed = I2C_MASTER_FREQ_HZ}};
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK)
    {
        return err;
    }
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode,
                              I2C_MASTER_RX_BUF_DISABLE,
                              I2C_MASTER_TX_BUF_DISABLE, 0);
}

// Hàm ghi dữ liệu vào thanh ghi
esp_err_t mpr121_write_reg(uint8_t reg_addr, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPR121_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

// Hàm đọc dữ liệu từ thanh ghi
esp_err_t mpr121_read_reg(uint8_t reg_addr, uint8_t *data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPR121_ADDR << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (MPR121_ADDR << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, data, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

// Hàm khởi tạo MPR121
esp_err_t mpr121_init(void)
{
    esp_err_t ret;

    // Soft reset
    ret = mpr121_write_reg(0x80, 0x63);
    if (ret != ESP_OK)
        return ret;
    vTaskDelay(pdMS_TO_TICKS(100));

    // Cấu hình các ngưỡng touch/release
    for (uint8_t i = 0; i < 12; i++)
    {
        ret = mpr121_write_reg(ELE0_TOUCH_THRESHOLD + i * 2, 12);
        if (ret != ESP_OK)
            return ret;
        ret = mpr121_write_reg(ELE0_RELEASE_THRESHOLD + i * 2, 6);
        if (ret != ESP_OK)
            return ret;
    }

    // Cấu hình các thông số lọc
    ret = mpr121_write_reg(0x5C, 0x10);
    if (ret != ESP_OK)
        return ret;
    ret = mpr121_write_reg(0x5D, 0x24);
    if (ret != ESP_OK)
        return ret;

    // Enable tất cả điện cực
    ret = mpr121_write_reg(0x5E, 0x0C);
    return ret;
}

// Hàm đọc trạng thái touch
esp_err_t mpr121_read_touch_status(uint16_t *touch_status)
{
    uint8_t data[2];
    esp_err_t ret;

    // Đọc 2 byte trạng thái
    for (int i = 0; i < 2; i++)
    {
        ret = mpr121_read_reg(TOUCH_STATUS_REG + i, &data[i]);
        if (ret != ESP_OK)
            return ret;
    }

    *touch_status = (data[1] << 8) | data[0];
    return ESP_OK;
}

// Hàm kiểm tra một phím cụ thể
bool mpr121_is_touched(uint16_t touch_status, uint8_t pin)
{
    if (pin > 11)
        return false;
    return (touch_status & (1 << pin));
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

static void touch_event_task(void *pvParameters)
{
    // key_state_t *switch_states = (key_state_t *)pvParameters;
    uint16_t touch_status;

    // key_state_t key_states[4] = {0};  // Khởi tạo trạng thái cho 12 phím

    while (1)
    {
        esp_err_t ret = mpr121_read_touch_status(&touch_status);
        if (ret == ESP_OK)
        {
            uint32_t current_time = pdTICKS_TO_MS(xTaskGetTickCount());
            // mutex = xSemaphoreCreateMutex();
            // Kiểm tra từng phím
            // if (xSemaphoreTake(mutex, portMAX_DELAY) == pdTRUE)
            // {
            for (int i = 0; i < MAX_SWITCH; i++)
            {
                bool is_touched = mpr121_is_touched(touch_status, i);

                // Phím đang được nhấn
                if (is_touched)
                {
                    // Phát hiện rising edge
                    if (!switch_states[i].is_pressed)
                    {
                        switch_states[i].is_pressed = true;
                        switch_states[i].press_start_time = current_time;
                        switch_states[i].long_press_triggered = false;
                        switch_states[i].short_press_triggered = false;
                    }
                    // Phím đang giữ
                    else
                    {
                        uint32_t press_duration = current_time - switch_states[i].press_start_time;

                        // Chỉ xử lý long press cho key 0
                        if (i == 0 && !switch_states[i].long_press_triggered)
                        {
                            if (press_duration >= LONG_PRESS_TIME_MS)
                            {
                                ESP_LOGI(TAG, "Key 0 LONG PRESS detected!");
                                switch_states[i].long_press_triggered = true;
                                saved_mode = !saved_mode;
                                ESP_LOGI(TAG, "saved mode: %d", saved_mode);

                                esp_err_t err = save_mode_config_to_nvs(&saved_mode);
                                if (err != ESP_OK)
                                {
                                    ESP_LOGE(TAG, "Failed to save mode to NVS: %s", esp_err_to_name(err));
                                    return; // Có thể không restart nếu lưu thất bại
                                }

                                esp_restart();
                            }
                        }
                        else if (i == 1 && !switch_states[i].long_press_triggered)
                        {
                            if (press_duration >= LONG_PRESS_TIME_MS)
                            {
                                ESP_LOGI(TAG, "Key 1 LONG PRESS detected!");
                                esp_err_t err = clear_all_nvs();
                            }
                        }
                    }
                }
                // Phím vừa được thả ra
                else if (switch_states[i].is_pressed)
                {
                    uint32_t press_duration = current_time - switch_states[i].press_start_time;

                    // Xử lý short press cho tất cả các phím
                    if (!switch_states[i].long_press_triggered &&
                        press_duration < SHORT_PRESS_TIME_MS)
                    {
                        ESP_LOGI(TAG, "Key %d SHORT PRESS detected!", i);

                        if (saved_mode == true)
                        {
                            switch_states[i].state = !switch_states[i].state;
                            ESP_LOGI(TAG, "Key %d state %d", i, switch_states[i].state);
                            control_gpio_led(i, switch_states[i].state);
                            publish_electrode_state(i, switch_states[i].state);
                        }
                        else
                        {
                            uint16_t endpoint_id;
                            switch (i)
                            {
                            case 0:
                                endpoint_id = light_endpoint_id1;
                                break;
                            case 1:
                                endpoint_id = light_endpoint_id2;
                                break;
                            case 2:
                                endpoint_id = light_endpoint_id3;
                                break;
                            case 3:
                                endpoint_id = light_endpoint_id4;
                                break;
                            default:
                                return;
                            }

                            // Đảo trạng thái
                            switch_states[i].state = !switch_states[i].state;

                            // Tạo val mới từ trạng thái đã đảo
                            esp_matter_attr_val_t val = esp_matter_bool(switch_states[i].state);

                            // Cập nhật giá trị attribute trong Matter
                            attribute::update(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);

                            // Cập nhật LED
                            app_driver_attribute_update(NULL, endpoint_id, OnOff::Id,
                                                        OnOff::Attributes::OnOff::Id, &val);

                            ESP_LOGI(TAG, "Key %d state updated to %d", i, switch_states[i].state);
                        }
                    }

                    // Reset trạng thái
                    switch_states[i].is_pressed = false;
                }
            }
            // xSemaphoreGive(mutex);
            // }
        }
        else
        {
            ESP_LOGE(TAG, "Error reading touch status: %s", esp_err_to_name(ret));
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Giảm delay để phản hồi nhanh hơn
    }
}

void start_touch_event_task()
{
    xTaskCreate(touch_event_task, "mpr121_task", 4096, NULL, 15, NULL);
}