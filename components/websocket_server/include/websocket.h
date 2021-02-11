/*
 *  Copyright: Collin Mulliner
 *
 *  based on: https://github.com/ThomasBarth/WebSockets-on-the-ESP32/blob/master/main/WebSocket_Task.c
 *  license: MIT
 */

#ifndef _WEBSOCKET_H_
#define _WEBSOCKET_H_

typedef struct
{
    uint32_t payload_length;
    char *payload;
} websocket_frame_t;

// return = 0 for success, return != 0 will close connection
typedef int websocket_server_new_frame_callback(websocket_frame_t *, const void *);

// 1 = connected, 0 = disconnected
typedef void websocket_server_conninfo_callback(int);

typedef int (*_log_printf_func_t)(const char *, ...);

typedef struct
{
    uint16_t port;

    websocket_server_new_frame_callback *callback;
    void *callback_data;
    websocket_server_conninfo_callback *conninfo_callback;

    unsigned long server_timeout;
    unsigned long client_timeout;

    _log_printf_func_t log_printf;
} websocket_server_config_t;

/*
 * websocket_server_config *ws_server_cfg = malloc(sizeof(websocket_server_config_t));
 * ws_server_cfg->port = 1337;
 * ws_server_cfg->callback = my_callback;
 * ws_server_cfg->callback_data = NULL; // set callback data
 * ws_server_cfg->conninfo_callback = NULL;
 * websocket_start(ws_server_cfg);
 */

int websocket_send(const char *data, const size_t length);
int websocket_start(websocket_server_config_t *ws_server_cfg);
void websocket_stop();
int websocket_is_connected();
char *websocket_get_client_ip();

//#define WS_DEBUG 1

#endif
