#include "nvs_manager.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"

esp_err_t nvs_manager_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

esp_err_t save_wifi_config_to_nvs(const char *ssid, const char *password)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(my_handle, "wifi_ssid", ssid);
    if (err != ESP_OK)
        return err;

    err = nvs_set_str(my_handle, "wifi_password", password);
    if (err != ESP_OK)
        return err;

    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        return err;

    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t load_wifi_config_from_nvs(char *ssid, char *password)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    size_t required_size;
    err = nvs_get_str(my_handle, "wifi_ssid", NULL, &required_size);
    if (err == ESP_OK)
    {
        nvs_get_str(my_handle, "wifi_ssid", ssid, &required_size);
    }

    err = nvs_get_str(my_handle, "wifi_password", NULL, &required_size);
    if (err == ESP_OK)
    {
        nvs_get_str(my_handle, "wifi_password", password, &required_size);
    }

    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t save_mode_config_to_nvs(bool *mode)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    uint8_t value = (*mode) ? 1 : 0;
    err = nvs_set_u8(my_handle, "mode", value);
    if (err != ESP_OK)
        return err;

    err = nvs_commit(my_handle);
    if (err != ESP_OK)
        return err;

    nvs_close(my_handle);
    return ESP_OK;
}

esp_err_t load_mode_config_from_nvs(bool *mode)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    uint8_t value;

    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
        return err;

    err = nvs_get_u8(my_handle, "mode", &value);
    if (err == ESP_OK)
    {
        *mode = (value != 0);
    }

    nvs_close(my_handle);
    return err; // Return actual error code
}

esp_err_t clear_all_nvs(void)
{

    // Xóa toàn bộ NVS partition
    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK)
    {
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Delay nhỏ để đảm bảo log được in ra
    esp_restart();                  // Restart ESP32

    return ESP_OK; // Dòng này sẽ không bao giờ được thực thi do đã restart
}