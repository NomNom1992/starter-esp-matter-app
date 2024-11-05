#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
// #include "nvs_manager.h"
#include "wifi_manager.h"

#include "esp_netif.h"
#include "lwip/inet.h"

#include <string.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/event_groups.h"
// #include "regex.h"

#include "cJSON.h"
#include "public.h"
// #include "freertos/task.h"

static const char *TAG = "WIFI123";

int retry_num = 0;
bool is_wifi_connected = false;

void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d, reason=%d",
                 MAC2STR(event->mac), event->aid, event->reason);
    }
    if (event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WIFI CONNECTING....\n");
    }
    else if (event_id == WIFI_EVENT_STA_CONNECTED)
    {
        is_wifi_connected = true;
        ESP_LOGI(TAG, "WIFI CONNECTED\n");
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "wifi lost connection");
        if (retry_num < 50)
        {
            esp_wifi_connect();
            retry_num++;
            ESP_LOGI(TAG, "WIFI RETRY CONNECTING....\n");
        }
    }
    else if (event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "WIFI GOT IP\n");
    }
}

void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

void wifi_connection(const char *ssid, const char *pass)
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t wifi_initiation = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&wifi_initiation);
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL);
    wifi_config_t wifi_configuration = {
        .sta = {
            .ssid = "",
            .password = "",
        }};
    strcpy((char *)wifi_configuration.sta.ssid, ssid);
    strcpy((char *)wifi_configuration.sta.password, pass);
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_configuration);
    esp_wifi_start();
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_connect();
    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:%s  password:%s\n", ssid, pass);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

static void wifi_scan_task(void *pvParameters)
{
    char *result = NULL;
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t *ap_info = malloc(sizeof(wifi_ap_record_t) * DEFAULT_SCAN_LIST_SIZE);

    if (!ap_info)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for AP info");
        vTaskDelete(NULL);
        return;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));

    // Tạo JSON và lưu kết quả
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < number; i++)
    {
        cJSON *wifi = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi, "ssid", (char *)ap_info[i].ssid);
        cJSON_AddNumberToObject(wifi, "rssi", ap_info[i].rssi);
        cJSON_AddItemToArray(root, wifi);
    }

    result = cJSON_Print(root);

    // Cleanup
    cJSON_Delete(root);
    free(ap_info);
    esp_wifi_stop();
    esp_wifi_deinit();

    // Lưu kết quả vào biến toàn cục hoặc gửi qua queue
    if (json_string != NULL)
    {
        free(json_string);
    }
    json_string = result;

    vTaskDelete(NULL);
}

char *perform_wifi_scan(void)
{
    xTaskCreate(wifi_scan_task, "wifi_scan", 4096, NULL, 5, NULL);

    // Đợi kết quả scan
    int retry = 0;
    while (json_string == NULL && retry < 10)
    {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        retry++;
    }

    return json_string;
}

bool get_wifi_connect_state(void)
{
    return is_wifi_connected;
}