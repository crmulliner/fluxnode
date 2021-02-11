/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef _BUTTON_SERVICE_H_
#define _BUTTON_SERVICE_H_

typedef int button_service_callback(int presses, unsigned long timeout_msec, void *data);

void button_service_set_cb(button_service_callback cb, void *cb_data);
void button_server_set_timeout(unsigned long timeout_msec);
void button_service_start(button_service_callback cb, void *cb_data);
void button_service_stop();
void button_service_enable(int enable);
int button_service_ispressed();

// we record num presses over this time (MS)
#define BUTTON_SERVICE_DEFAULT_TIMEOUT 3000
#define BUTTON_PRESS_NUM_MAX 11
// only one press within this time
#define BUTTON_PRESS_MIN_MS pdMS_TO_TICKS(300)

#endif
