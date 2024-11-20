#include "esp_log.h"
#include "mqtt_service.h"
#include "public.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "gpio_handle.h"
// #include "mpr121_handle.h"

static const char *TAG = "MQTT_CLIENT";

#define MQTT_BROKER_URL "mqtt://mqtt.thanhtoanqrcode.vn:1883"

static esp_mqtt_client_handle_t client;
// static QueueHandle_t touch_event_queue;

// Define command topics and state topics
#define SWITCH_COMMAND_TOPIC_1 "google/home/commands/switch/1434TYRE/onoff/set"
#define SWITCH_STATE_TOPIC_1 "google/home/state/switch/1434TYRE/onoff"
#define SWITCH_PING_TOPIC_1 "google/home/ping/switch/1434TYRE"

#ifdef ENABLE_TOPIC_2
#define SWITCH_COMMAND_TOPIC_2 "google/home/commands/outlet/AGF65T/onoff/set"
#define SWITCH_STATE_TOPIC_2 "google/home/state/outlet/AGF65T/onoff"
#endif

#ifdef ENABLE_TOPIC_3
#define SWITCH_COMMAND_TOPIC_3 "google/home/commands/switch/2NDSWITCH/onoff/set"
#define SWITCH_STATE_TOPIC_3 "google/home/state/switch/2NDSWITCH/onoff"
#endif

#ifdef ENABLE_TOPIC_4
#define SWITCH_COMMAND_TOPIC_4 "google/home/commands/outlet/2NDOUTLET/onoff/set"
#define SWITCH_STATE_TOPIC_4 "google/home/state/outlet/2NDOUTLET/onoff"
#endif

void publish_electrode_state(int index, bool state)
{
    const char *state_str = state ? "on" : "off";
    const char *topic;
    switch (index)
    {
    case 0:
        topic = SWITCH_STATE_TOPIC_1;
        break;
#ifdef ENABLE_TOPIC_2
    case 1:
        topic = SWITCH_STATE_TOPIC_2;
        break;
#endif
#ifdef ENABLE_TOPIC_3
    case 2:
        topic = SWITCH_STATE_TOPIC_3;
        break;
#endif
#ifdef ENABLE_TOPIC_4
    case 3:
        topic = SWITCH_STATE_TOPIC_4;
        break;
#endif
    default:
        ESP_LOGE(TAG, "Invalid electrode index");
        return;
    }
    int msg_id = esp_mqtt_client_publish(client, topic, state_str, 0, 1, 0);
    if (msg_id != -1)
    {
        ESP_LOGI(TAG, "Published state for electrode %d: %s, msg_id=%d", index, state_str, msg_id);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to publish state for electrode %d", index);
    }
}

void handle_mqtt_data(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "MQTT_EVENT_DATA");

    if (strncmp(event->topic, SWITCH_COMMAND_TOPIC_1, event->topic_len) == 0)
    {
        handle_electrode_command(event, 0);
    }
#ifdef ENABLE_TOPIC_2
    else if (strncmp(event->topic, SWITCH_COMMAND_TOPIC_2, event->topic_len) == 0)
    {
        handle_electrode_command(event, 1);
    }
#endif
#ifdef ENABLE_TOPIC_3
    else if (strncmp(event->topic, SWITCH_COMMAND_TOPIC_3, event->topic_len) == 0)
    {
        handle_electrode_command(event, 2);
    }
#endif
#ifdef ENABLE_TOPIC_4
    else if (strncmp(event->topic, SWITCH_COMMAND_TOPIC_4, event->topic_len) == 0)
    {
        handle_electrode_command(event, 3);
    }
#endif
}

void handle_electrode_command(esp_mqtt_event_handle_t event, int switch_index)
{
    if (strncmp(event->data, "on", event->data_len) == 0)
    {
        switch_states[switch_index].state = true;

        control_gpio_led(switch_index, switch_states[switch_index].state);

        ESP_LOGI(TAG, "Received command for electrode %d: ON", switch_index);
    }
    else if (strncmp(event->data, "off", event->data_len) == 0)
    {
        switch_states[switch_index].state = false;

        control_gpio_led(switch_index, switch_states[switch_index].state);

        ESP_LOGI(TAG, "Received command for electrode %d: OFF", switch_index);
    }
    publish_electrode_state(switch_index, switch_states[switch_index].state);
}

void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        esp_mqtt_client_subscribe(client, SWITCH_COMMAND_TOPIC_1, 0);
#ifdef ENABLE_TOPIC_2
        esp_mqtt_client_subscribe(client, SWITCH_COMMAND_TOPIC_2, 0);
#endif
#ifdef ENABLE_TOPIC_3
        esp_mqtt_client_subscribe(client, SWITCH_COMMAND_TOPIC_3, 0);
#endif
#ifdef ENABLE_TOPIC_4
        esp_mqtt_client_subscribe(client, SWITCH_COMMAND_TOPIC_4, 0);
#endif
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        handle_mqtt_data(event);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

esp_err_t mqtt_client_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return esp_mqtt_client_start(client);
}

esp_err_t mqtt_update_electrode_state(int electrode_index, bool new_state)
{
    if (switch_states[electrode_index].state != new_state)
    {
        switch_states[electrode_index].state = new_state;
        // publish_electrode_state(electrode_index);
        return ESP_OK;
    }
    return ESP_OK; // No change, no publish
}

bool mqtt_get_electrode_state(int electrode_index)
{
    if (electrode_index < 0 || electrode_index >= MAX_SWITCH)
    {
        ESP_LOGE(TAG, "Invalid electrode index");
        return false;
    }
    return switch_states[electrode_index].state;
}

// static void publish_task(void *pvParameters)
// {
//     ESP_LOGI(TAG, "Inside publish task");
//     TouchEvent *event = (TouchEvent*)pvParameters;
//     while (1) {
//         if (event->touch_event_queue == NULL) {
//             ESP_LOGE(TAG, "Touch event queue is NULL");
//             vTaskDelay(pdMS_TO_TICKS(10));
//             continue;
//         }

// if (xQueueReceive(event->touch_event_queue, &event, portMAX_DELAY) == pdTRUE) {
//     ESP_LOGI(TAG, "Publishing: Electrode %d is now %s",
//              event->electrode, event->state ? "touched" : "released");

//     mqtt_update_electrode_state(event->electrode, event->state);
// }
//         ESP_LOGI(TAG, "MQTT task loop end");
//         vTaskDelay(pdMS_TO_TICKS(20));
//     }
// }