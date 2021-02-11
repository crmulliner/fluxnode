/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

#include "log.h"
#include "wifi_service.h"

//#define WIFI_DEBUG 1

#define STATION_MAXIMUM_RETRY 3
#define AP_MAX_STA_CONN 1

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static int wifi_mode = 0;
// network interface
static esp_netif_t *net;

static char ip_address[16] = {0};

char *wifi_service_get_ip()
{
    return ip_address;
}

static void station_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < STATION_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
#ifdef WIFI_DEBUG
            logprintf("retry to connect to the AP\n");
#endif
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
#ifdef WIFI_DEBUG
        logprintf("connect to the AP fail\n");
#endif
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        memset(ip_address, 0, sizeof(ip_address));
        sprintf(ip_address, "%d.%d.%d.%d", IP2STR(&event->ip_info.ip));

#ifdef WIFI_DEBUG
        logprintf("IP: %s\n", ip_address);
#endif

        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static int wifi_init_station(char *ssid, char *password)
{
    s_wifi_event_group = xEventGroupCreate();

    net = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &station_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &station_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
        },
    };
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

#ifdef WIFI_DEBUG
    logprintf("wifi_init_sta finished\n");
#endif

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    int ret = 0;
    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
#ifdef WIFI_DEBUG
        logprintf("connected to ap SSID: '%s' password: '%s'\n", ssid, password);
#endif
        ret = 1;
    }
    else if (bits & WIFI_FAIL_BIT)
    {
#ifdef WIFI_DEBUG
        logprintf("Failed to connect to SSID: '%s' password: '%s'\n", ssid, password);
#endif
    }
    else
    {
#ifdef WIFI_DEBUG
        logprintf("Wifi UNEXPECTED EVENT\n");
#endif
    }

    // we should need this in order to stay connected?
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &station_event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &station_event_handler));
    vEventGroupDelete(s_wifi_event_group);

    return ret;
}

static void ap_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
#ifdef WIFI_DEBUG
        logprintf("station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
#endif
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
#ifdef WIFI_DEBUG
        logprintf("station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
#endif
    }
}

static int wifi_init_softap(char *ssid, char *password)
{
    net = esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &ap_event_handler, NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = strlen(ssid),
            .password = "",
            .max_connection = AP_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    strcpy((char *)wifi_config.ap.ssid, ssid);
    strcpy((char *)wifi_config.ap.password, password);

    if (strlen(password) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

#ifdef WIFI_DEBUG
    logprintf("wifi_init_softap finished. SSID: '%s' password: '%s'\n", ssid, password);
#endif
    return 1;
}

int wifi_start(wifi_service_mode_t mode, char *ssid, char *password)
{
#ifdef WIFI_DEBUG
    logprintf("%s\n", __func__);
#endif

    wifi_mode = mode;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    if (mode == wifi_mode_ap)
    {
        // AP mode we know our IP
        strcpy(ip_address, "192.168.4.1");
        wifi_init_softap(ssid, password);
        return 1;
    }
    else
    {
        return wifi_init_station(ssid, password);
    }
}

void wifi_stop()
{
    if (wifi_mode == wifi_mode_sta)
    {
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        vTaskDelay(100);
    }
    else
    {
        // TODO: fix AP mode stop
        vTaskDelay(100);
    }
    ESP_ERROR_CHECK(esp_wifi_stop());
    vTaskDelay(100);
    ESP_ERROR_CHECK(esp_wifi_deinit());
    vTaskDelay(100);

    esp_netif_destroy(net);
    vTaskDelay(100);
    esp_netif_deinit();
    vTaskDelay(100);

    esp_event_loop_delete_default();
    vTaskDelay(100);
}
