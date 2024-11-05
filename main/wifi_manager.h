#ifdef __cplusplus
extern "C"
{
#endif

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

// #include <stdbool.h>
#include "esp_err.h"
#include "esp_wifi.h"

#include "freertos/task.h"
#include "esp_system.h"

#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN

#define DEFAULT_SCAN_LIST_SIZE 20

    void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
    void wifi_init_softap(void);
    void wifi_connection(const char *ssid, const char *pass);
    char *perform_wifi_scan(void);
    bool get_wifi_connect_state(void);
    // void connect_wifi_task(void *pvParameters);

#endif // WIFI_MANAGER_H

#ifdef __cplusplus
}
#endif