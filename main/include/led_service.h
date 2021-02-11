/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef _LED_SERVICE_H_
#define _LED_SERVICE_H_

enum BLINK_T {
    SLOW = 0,
    MEDIUM,
    FAST,
    NOT
};

enum LED_ID_T {
    RED = 0,
    GREEN
};

void led_set(enum LED_ID_T id, int mode);

// ID = 0-1, speed = BLINK_T, brigtness = 0.0-1.0
void led_service_blink_led(enum LED_ID_T id, enum BLINK_T speed, float brightness);
void led_service_blink_led_stop(enum LED_ID_T id);

void led_service_init(int red, int green);

#endif
