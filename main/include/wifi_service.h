/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef _WIFI_SERVICE_H_
#define _WIFI_SERVICE_H_

typedef enum
{
    wifi_mode_ap = 0,
    wifi_mode_sta,
} wifi_service_mode_t;

void wifi_stop();
int wifi_start(wifi_service_mode_t mode, char *ssid, char *password);
char *wifi_service_get_ip();

#endif
