/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef _UTIL_H_
#define _UTIL_H_

// precent 0-100
int battery_percent();
float battery_mvolt();

int usb_isconnected();
int batt_charging();

void util_init();

#endif
