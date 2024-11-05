#ifdef __cplusplus
extern "C"
{
#endif
#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include "esp_err.h"
#include "stdbool.h"

    esp_err_t nvs_manager_init(void);
    esp_err_t save_wifi_config_to_nvs(const char *ssid, const char *password);
    esp_err_t load_wifi_config_from_nvs(char *ssid, char *password);
    esp_err_t save_mode_config_to_nvs(bool *mode);
    esp_err_t load_mode_config_from_nvs(bool *mode);

    esp_err_t clear_all_nvs(void);

#endif // NVS_MANAGER_H
#ifdef __cplusplus
}
#endif