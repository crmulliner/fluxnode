/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef __duk_main_h__
#define __duk_main_h__

#include <stdio.h>

#include "freertos/FreeRTOS.h"

typedef enum
{
    LORA_MSG = 0,
    UI_MSG,
    UI_CONNECTED,
    UI_DISCONNECTED,
    BUTTON_PRESSED,
    // not implemented yet
    USB_CONNECTED,
    USB_DISCONNECTED,
    BATT_CHARGING,
    BATT_DRAINING,
} event_msg_type;

typedef enum
{
    INCOMING = 0,
    OUTGOING = 1,
} event_direction_type;

typedef int ui_msg_send_func(const uint8_t *buffer, const size_t len);

int duk_main_add_full_event(event_msg_type msg_type, const event_direction_type direction, uint8_t *payload, const size_t len, const int rssi, const int snr, const time_t ts);
int duk_main_add_event(event_msg_type msg_type, event_direction_type direction, uint8_t *payload, size_t len);
void duk_main_start();
void duk_main_set_send_func(ui_msg_send_func *func);
void duk_main_set_reset(int rst);
int duk_main_set_wake_up_time(unsigned long int wake_up_timeMS);
void duk_main_set_load_file(char *fname);

#endif
