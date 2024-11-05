#ifdef __cplusplus
extern "C"
{
#endif

#ifndef MQTT_SERVICE_H
#define MQTT_SERVICE_H

#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "mqtt_client.h"
#include "public.h"

    // Khởi tạo và kết nối MQTT client
    esp_err_t mqtt_client_init(void);
    void handle_electrode_command(esp_mqtt_event_handle_t event, int electrode_index);

    esp_err_t mqtt_update_electrode_state(int electrode_index, bool new_state);
    bool mqtt_get_electrode_state(int electrode_index);
    // void mpr121_eventdata_to_mqtt(QueueHandle_t queue);
    void start_publish_event_task();
    void publish_electrode_state(int index, bool state);
#endif // MQTT_SERVICE_H

#ifdef __cplusplus
}
#endif