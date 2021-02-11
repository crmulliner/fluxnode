/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef __web_service_h__
#define __web_service_h__

typedef enum {
    CMD_NONE = 0,
    CMD_RESET,
    CMD_SET_LOAD,
    CMD_REBOOT,
    CMD_DELETE_FILE,
} control_cmd_t;

void webserver_stop();
int webserver_start(int read_only);

typedef int webserver_control_function_t(int, char*, size_t);

void webserver_set_control_func(webserver_control_function_t *func);

#endif
