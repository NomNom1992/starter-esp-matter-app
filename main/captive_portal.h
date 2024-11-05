#ifdef __cplusplus
extern "C"
{
#endif

#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "regex.h"

#include "lwip/err.h" //light weight ip packets error handling
#include "lwip/sys.h" //system applications for light weight ip apps"

    extern const char root_start[] asm("_binary_root_html_start");
    extern const char root_end[] asm("_binary_root_html_end");

    extern const uint8_t logo_jpg_start[] asm("_binary_logo_jpg_start");
    extern const uint8_t logo_jpg_end[] asm("_binary_logo_jpg_end");

    // const httpd_uri_t root;

    void start_captive_portal(void);
    void stop_captive_portal(void);
    bool capt_get_active_state(void);

#endif // CAPTIVE_PORTAL_H
#ifdef __cplusplus
}
#endif
