/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "esp_event.h"
#include "esp_mac.h"

#include "nvs_manager.h"
#include "wifi_manager.h"
#include "captive_portal.h"
#include "mqtt_service.h"
#include "mpr121_handle.h"

#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>

#include <esp_matter.h>
#include <esp_matter_console.h>
#include <esp_matter_ota.h>

#include <app_priv.h>
#include <app_reset.h>

#include "freertos/semphr.h"
#include "public.h"

#include <app/server/CommissioningWindowManager.h>
#include <app/server/Server.h>
#include <app/server/OnboardingCodesUtil.h>

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

bool saved_mode = true;
char saved_ssid[32] = {0};
char saved_password[64] = {0};
SemaphoreHandle_t mutex = NULL;
key_state_t switch_states[MAX_SWITCH] = {0};

static const char *TAG = "app_main";
// uint16_t light_endpoint_id = 0;
uint16_t light_endpoint_id1 = 0; // Endpoint cho đèn 1
uint16_t light_endpoint_id2 = 0; // Endpoint cho đèn 2
uint16_t light_endpoint_id3 = 0; // Endpoint cho đèn 3
uint16_t light_endpoint_id4 = 0; // Endpoint cho đèn 4

constexpr auto k_timeout_seconds = 300;

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type)
    {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged:
        ESP_LOGI(TAG, "Interface IP Address changed");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;

    case chip::DeviceLayer::DeviceEventType::kFailSafeTimerExpired:
        ESP_LOGI(TAG, "Commissioning failed, fail safe timer expired");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowOpened:
        ESP_LOGI(TAG, "Commissioning window opened");
        break;

    case chip::DeviceLayer::DeviceEventType::kCommissioningWindowClosed:
        ESP_LOGI(TAG, "Commissioning window closed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
    {
        ESP_LOGI(TAG, "Fabric removed successfully");
        if (chip::Server::GetInstance().GetFabricTable().FabricCount() == 0)
        {
            chip::CommissioningWindowManager &commissionMgr = chip::Server::GetInstance().GetCommissioningWindowManager();
            constexpr auto kTimeoutSeconds = chip::System::Clock::Seconds16(k_timeout_seconds);
            if (!commissionMgr.IsCommissioningWindowOpen())
            {
                /* After removing last fabric, this example does not remove the Wi-Fi credentials
                 * and still has IP connectivity so, only advertising on DNS-SD.
                 */
                CHIP_ERROR err = commissionMgr.OpenBasicCommissioningWindow(kTimeoutSeconds,
                                                                            chip::CommissioningWindowAdvertisement::kDnssdOnly);
                if (err != CHIP_NO_ERROR)
                {
                    ESP_LOGE(TAG, "Failed to open commissioning window, err:%" CHIP_ERROR_FORMAT, err.Format());
                }
            }
        }
        break;
    }

    case chip::DeviceLayer::DeviceEventType::kFabricWillBeRemoved:
        ESP_LOGI(TAG, "Fabric will be removed");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricUpdated:
        ESP_LOGI(TAG, "Fabric is updated");
        break;

    case chip::DeviceLayer::DeviceEventType::kFabricCommitted:
        ESP_LOGI(TAG, "Fabric is committed");
        break;

    case chip::DeviceLayer::DeviceEventType::kBLEDeinitialized:
        ESP_LOGI(TAG, "BLE deinitialized and memory reclaimed");
        break;

    default:
        break;
    }
}

// This callback is invoked when clients interact with the Identify Cluster.
// In the callback implementation, an endpoint can identify itself. (e.g., by flashing an LED or light).
static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id, uint8_t effect_id,
                                       uint8_t effect_variant, void *priv_data)
{
    ESP_LOGI(TAG, "Identification callback: type: %u, effect: %u, variant: %u", type, effect_id, effect_variant);
    return ESP_OK;
}

// This callback is called for every attribute update. The callback implementation shall
// handle the desired attributes and return an appropriate error code. If the attribute
// is not of your interest, please do not return an error code and strictly return ESP_OK.
static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id, uint32_t cluster_id,
                                         uint32_t attribute_id, esp_matter_attr_val_t *val, void *priv_data)
{
    esp_err_t err = ESP_OK;

    if (type == PRE_UPDATE)
    {
        /* Do stuff here */
        /* Driver update */
        app_driver_handle_t driver_handle = (app_driver_handle_t)priv_data;
        err = app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
    }

    return err;
}

static bool check_mode()
{
    esp_err_t err = load_mode_config_from_nvs(&saved_mode);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to load mode from NVS: %s", esp_err_to_name(err));
        saved_mode = false; // Default value
        return saved_mode;
    }

    if (saved_mode == false)
    {
        ESP_LOGI(TAG, "running with matter...");
    }
    else
    {
        ESP_LOGI(TAG, "running with MQTT...");
    }
    return saved_mode;
}

void init_gpio_leds()
{
    const gpio_num_t leds[] = {LED1, LED2, LED3, LED4};

    for (int i = 0; i < 4; i++)
    {
        esp_rom_gpio_pad_select_gpio(leds[i]);
        gpio_set_direction(leds[i], GPIO_MODE_OUTPUT);
        gpio_set_level(leds[i], 0); // Initialize all LEDs to off
    }
}

extern "C" void app_main()
{

    /* Initialize the ESP NVS layer */
    nvs_flash_init();

    if (check_mode())
    {
        // Khởi tạo network interface
        ESP_ERROR_CHECK(esp_netif_init());

        // Tạo event loop mặc định
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        // if (load_wifi_config_from_nvs(saved_ssid, saved_password) == ESP_OK && strlen(saved_ssid) > 0)
        // {
        //     // We have saved credentials, try to connect
        //     wifi_connection(saved_ssid, saved_password);
        // }
        if (true)
        {
            // We have saved credentials, try to connect
            wifi_connection("FFT-VT", "11235813");
        }
        else
        {
            esp_wifi_stop();
            esp_wifi_deinit();
            // No saved credentials, start captive portal
            start_captive_portal();
        }
        init_gpio_leds();
        vTaskDelay(500);

        // if (get_wifi_connect_state())
        ESP_LOGI(TAG, "mqtt start here.........");
        ESP_ERROR_CHECK(mqtt_client_init());

        ESP_LOGI(TAG, "Initializing I2C");
        ESP_ERROR_CHECK(i2c_master_init());

        ESP_LOGI(TAG, "Initializing MPR121");
        ESP_ERROR_CHECK(mpr121_init());

        ESP_LOGI(TAG, "touch event start here.........");
        start_touch_event_task();
    }
    else
    {
        esp_err_t err = ESP_OK;

        // Khởi tạo network interface
        ESP_ERROR_CHECK(esp_netif_init());

        // Tạo event loop mặc định
        ESP_ERROR_CHECK(esp_event_loop_create_default());

        // mutex = xSemaphoreCreateMutex();
        /* Initialize driver */
        app_driver_handle_t light_handle = app_driver_light_init();
        /* Create a Matter node and add the mandatory Root Node device type on endpoint 0 */
        node::config_t node_config;
        node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);

        /* Tạo endpoint cho đèn 1 */
        on_off_light::config_t light_config1;
        light_config1.on_off.on_off = DEFAULT_POWER;
        light_config1.on_off.lighting.start_up_on_off = nullptr;
        endpoint_t *endpoint1 = on_off_light::create(node, &light_config1, ENDPOINT_FLAG_NONE, light_handle);

        /* Tạo endpoint cho đèn 2 */
        on_off_plugin_unit::config_t light_config2;
        light_config2.on_off.on_off = DEFAULT_POWER;
        light_config2.on_off.lighting.start_up_on_off = nullptr;
        endpoint_t *endpoint2 = on_off_plugin_unit::create(node, &light_config2, ENDPOINT_FLAG_NONE, light_handle);

        /* Tạo endpoint cho đèn 3 */
        on_off_light::config_t light_config3;
        light_config3.on_off.on_off = DEFAULT_POWER;
        light_config3.on_off.lighting.start_up_on_off = nullptr;
        endpoint_t *endpoint3 = on_off_light::create(node, &light_config3, ENDPOINT_FLAG_NONE, light_handle);

        /* Tạo endpoint cho đèn 4 */
        on_off_plugin_unit::config_t light_config4;
        light_config4.on_off.on_off = DEFAULT_POWER;
        light_config4.on_off.lighting.start_up_on_off = nullptr;
        endpoint_t *endpoint4 = on_off_plugin_unit::create(node, &light_config4, ENDPOINT_FLAG_NONE, light_handle);
        /* Kiểm tra tạo endpoint thành công */
        if (!node || !endpoint1 || !endpoint2 || !endpoint3 || !endpoint4)
        {
            ESP_LOGE(TAG, "Matter node creation failed");
        }

        /* Lấy Endpoint Id */
        light_endpoint_id1 = endpoint::get_id(endpoint1);
        light_endpoint_id2 = endpoint::get_id(endpoint2);
        light_endpoint_id3 = endpoint::get_id(endpoint3);
        light_endpoint_id4 = endpoint::get_id(endpoint4);

        ESP_LOGI(TAG, "Lights created with endpoint_ids %d and %d", light_endpoint_id1, light_endpoint_id2);
        // ESP_LOGI(TAG, "Matter App created with endpoint_id");

        /* Matter start */
        err = esp_matter::start(app_event_cb);
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Matter start failed: %d", err);
        }

        /* Starting driver with default values */
        // app_driver_light_set_defaults(light_endpoint_id);
        /* Khởi tạo giá trị mặc định cho cả 2 đèn */
        app_driver_light_set_defaults(light_endpoint_id1);
        app_driver_light_set_defaults(light_endpoint_id2);
        app_driver_light_set_defaults(light_endpoint_id3);
        app_driver_light_set_defaults(light_endpoint_id4);

        PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kBLE));

        ESP_LOGI(TAG, "Initializing I2C");
        ESP_ERROR_CHECK(i2c_master_init());

        ESP_LOGI(TAG, "Initializing MPR121");
        ESP_ERROR_CHECK(mpr121_init());

        ESP_LOGI(TAG, "touch event start here.........");
        start_touch_event_task();
    }
}
