#include "captive_portal.h"
#include "cJSON.h"
#include "wifi_manager.h"
#include "nvs_manager.h"
#include "esp_http_server.h"
#include "dns_server.h"

static const char *TAG = "Captive";
char *json_string = NULL;
bool captive_portal_active = true;
httpd_handle_t server = NULL;
dns_server_handle_t dns_handle = NULL;

esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = root_end - root_start;

    ESP_LOGI(TAG, "Serve root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, root_start, root_len);

    return ESP_OK;
}

const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler};

// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static esp_err_t wifi_scan_get_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Handling WiFi scan request");
    if (json_string == NULL)
    {
        // Nếu có lỗi, trả về một mảng rỗng
        json_string = strdup("[]");
    }
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    ESP_LOGI(TAG, "WiFi scan result JSON in handler: %.100s", json_string);
    // free(json_string);

    return ESP_OK;
}

void connect_wifi_task(void *pvParameters)
{
    vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay to allow HTTP response to be sent

    // Stop captive portal
    stop_captive_portal();

    // Connect to WiFi
    char ssid[32];
    char password[64];
    load_wifi_config_from_nvs(ssid, password);
    wifi_connection(ssid, password);
    esp_restart();
}

static const httpd_uri_t wifi_scan = {
    .uri = "/wifi-scan",
    .method = HTTP_GET,
    .handler = wifi_scan_get_handler,
    .user_ctx = NULL};

static esp_err_t submit_post_handler(httpd_req_t *req)
{
    char content[100];
    size_t recv_size = MIN(req->content_len, sizeof(content));

    int ret = httpd_req_recv(req, content, recv_size);
    if (ret <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    content[recv_size] = '\0';
    ESP_LOGI(TAG, "Received form data: %s", content);

    // Parse the received data
    char wifi[32] = {0};
    char password[64] = {0};

    sscanf(content, "wifi=%31[^&]&password=%63[^&]", wifi, password);

    // Lưu cấu hình Wi-Fi vào NVS
    esp_err_t err = save_wifi_config_to_nvs(wifi, password);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save WiFi config to NVS: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "WiFi config saved to NVS successfully");
    }

    // Set flag to switch mode
    captive_portal_active = false;

    // Respond to the client
    // Tạo HTML response
    const char *resp_str =
        "<html>"
        "<head>"
        "<meta charset='UTF-8'>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<style>"
        "body {"
        "    display: flex;"
        "    justify-content: center;"
        "    align-items: center;"
        "    height: 100vh;"
        "    margin: 0;"
        "    font-family: Arial, sans-serif;"
        "    background-color: #f0f0f0;"
        "}"
        ".success-container {"
        "    text-align: center;"
        "    background-color: white;"
        "    padding: 20px;"
        "    border-radius: 10px;"
        "    box-shadow: 0 0 10px rgba(0,0,0,0.1);"
        "}"
        ".checkmark {"
        "    color: #4CAF50;"
        "    font-size: 48px;"
        "}"
        "</style>"
        "</head>"
        "<body>"
        "<div class='success-container'>"
        "<div class='checkmark'>&#10004;</div>"
        "<h2>Đã gửi thành công</h2>"
        "</div>"
        "</body>"
        "</html>";

    // Gửi response
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, resp_str, strlen(resp_str));

    // Start a task to handle WiFi connection
    xTaskCreate(&connect_wifi_task, "connect_wifi_task", 4096, NULL, 5, NULL);

    return ESP_OK;
}

static const httpd_uri_t submit = {
    .uri = "/submit",
    .method = HTTP_POST,
    .handler = submit_post_handler,
    .user_ctx = NULL};

static esp_err_t logo_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_send(req, (const char *)logo_jpg_start, logo_jpg_end - logo_jpg_start);
    return ESP_OK;
}

static const httpd_uri_t logo = {
    .uri = "/logo.jpg",
    .method = HTTP_GET,
    .handler = logo_get_handler,
    .user_ctx = NULL};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &wifi_scan);
        httpd_register_uri_handler(server, &submit);
        httpd_register_uri_handler(server, &logo);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

void start_captive_portal(void)
{
    // Cấu hình logging
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    // Quét Wi-Fi
    perform_wifi_scan();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // Tạo network interface mặc định cho Wi-Fi AP

    esp_netif_create_default_wifi_ap();

    // Khởi tạo Wi-Fi ở chế độ SoftAP
    wifi_init_softap();

    // Khởi động web server
    server = start_webserver();

    // Khởi động DNS server
    dns_server_config_t dns_config = DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
    dns_handle = start_dns_server(&dns_config);

    ESP_LOGI(TAG, "Captive portal started successfully");
}

void stop_captive_portal(void)
{
    if (server)
    {
        httpd_stop(server);
        server = NULL;
    }
    if (dns_handle)
    {
        stop_dns_server(dns_handle);
        dns_handle = NULL;
    }
    esp_wifi_stop();
    esp_wifi_deinit();
    ESP_LOGI(TAG, "Captive portal stopped");
    vTaskDelay(1000);
}

bool capt_get_active_state(void)
{
    return captive_portal_active;
}