/*
 *  Copyright: Collin Mulliner
 *
 *  based on: https://github.com/ThomasBarth/WebSockets-on-the-ESP32/blob/master/main/WebSocket_Task.c
 *  license: MIT
 */

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "esp32/sha.h"
#include "esp_system.h"
#include "mbedtls/base64.h"
#include "lwip/api.h"

#include "websocket.h"

typedef enum
{
    WS_OP_CONNECT = 0x0,
    WS_OP_TEXT = 0x1,
    WS_OP_BIN = 0x2,
    WS_OP_CLOSE = 0x8,
    WS_OP_PING = 0x9,
    WS_OP_PONG = 0xA
} websocket_opcodes_t;

typedef struct __attribute__((__packed__))
{
    uint8_t opcode : 4;
    uint8_t reserved : 3;
    uint8_t FIN : 1;
    uint8_t payload_len : 7;
    uint8_t mask : 1;
} websocket_frame_header_t;

typedef struct __attribute__((__packed__))
{
    uint8_t opcode : 4;
    uint8_t reserved : 3;
    uint8_t FIN : 1;
    uint8_t payload_len : 7;
    uint8_t mask : 1;
    uint16_t len;
} websocket_frame_header_16len_t;

typedef struct __attribute__((__packed__))
{
    uint8_t opcode : 4;
    uint8_t reserved : 3;
    uint8_t FIN : 1;
    uint8_t payload_len : 7;
    uint8_t mask : 1;
    uint32_t len;
} websocket_frame_header_32len_t;

static struct netconn *websocket_conn;
struct netconn *server_conn;
static int websocket_server_running;
static char client_ip_address[16];

const char websocket_sec_key[] = "Sec-WebSocket-Key:";
const char websocket_sec_con_key[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const char websocket_server_header[] = "HTTP/1.1 101 Switching Protocols \r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %.*s\r\n\r\n";

#define KEY_DIGEST_SIZE_MAX 24
#define SHA1_DIGEST_LEN 20
#define BASE64_DIGEST_LEN 42
#define TWOBYTE_LEN 2
#define FOURBYTE_LEN 4

char *websocket_get_client_ip()
{
    return client_ip_address;
}

static void conninfo_notify(websocket_server_config_t *cfg, int status)
{
    if (cfg->conninfo_callback)
    {
        cfg->conninfo_callback(status);
    }
}

static int websocket_handshake(struct netconn *conn, websocket_server_config_t *cfg)
{
    struct netbuf *inbuf;

    int err = netconn_recv(conn, &inbuf);

    if (err != ERR_OK)
    {
#ifdef WS_DEBUG
        cfg->log_printf("%s: recv failed, err = %d\n", __func__, err);
#endif
        return err;
    }

    char *buf;
    uint16_t buf_len;

    err = netbuf_data(inbuf, (void **)&buf, &buf_len);
    if (err != ERR_OK)
    {
#ifdef WS_DEBUG
        cfg->log_printf("%s: recv data, err = %d\n", __func__, err);
#endif
        return err;
    }

    char *seckey = strstr(buf, websocket_sec_key);

    if (seckey == NULL)
    {
        netbuf_delete(inbuf);
        return ERR_CONN;
    }

    char *handshake = malloc(sizeof(websocket_sec_con_key) + KEY_DIGEST_SIZE_MAX);
    if (handshake == NULL)
    {
        netbuf_delete(inbuf);
        return ERR_MEM;
    }
    memset(handshake, 0, sizeof(websocket_sec_con_key) + KEY_DIGEST_SIZE_MAX);

    int hs_len = 0;
    for (hs_len = 0; hs_len < KEY_DIGEST_SIZE_MAX; hs_len++)
    {
        if (*(seckey + sizeof(websocket_sec_key) + hs_len) == '\r' || *(seckey + sizeof(websocket_sec_key) + hs_len) == '\n')
        {
            break;
        }
        handshake[hs_len] = *(seckey + sizeof(websocket_sec_key) + hs_len);
    }
    memcpy(&handshake[hs_len], websocket_sec_con_key, sizeof(websocket_sec_con_key));
    netbuf_delete(inbuf);

    unsigned char *digest = malloc(SHA1_DIGEST_LEN + 1);

    // some bug here??
    esp_sha(SHA1, (unsigned char *)handshake, strlen(handshake), (unsigned char *)digest);

    char *base64digest = malloc(BASE64_DIGEST_LEN);
    size_t base64len = 0;
    memset(base64digest, 0, BASE64_DIGEST_LEN);
    mbedtls_base64_encode((unsigned char *)base64digest, BASE64_DIGEST_LEN, (size_t *)&base64len, (unsigned char *)digest, SHA1_DIGEST_LEN);

    free(digest);
    free(handshake);

    handshake = malloc(sizeof(websocket_server_header) + base64len + 1);
    if (handshake == NULL)
    {
        free(base64digest);
        return ERR_MEM;
    }

    sprintf(handshake, websocket_server_header, base64len, base64digest);
    free(base64digest);

    err = netconn_write(conn, handshake, strlen(handshake), NETCONN_COPY);
    if (err != ERR_OK)
    {
#ifdef WS_DEBUG
        cfg->log_printf("%s: write failed, err = %d\n", __func__, err);
#endif
    }
    free(handshake);

    // connection is established
    return err;
}

static int websocket_serve(struct netconn *conn, websocket_server_config_t *cfg)
{
    struct netbuf *inbuf;

    while (1)
    {
        int res = netconn_recv(conn, &inbuf);

        if (res != ERR_OK && websocket_server_running)
        {
            cfg->log_printf("%s: recv timeout (disconnecting)\n", __func__);
            break;
        }

        if (websocket_server_running == 0)
        {
            break;
        }

        char *buf;
        uint16_t buf_len;
        int err;

        err = netbuf_data(inbuf, (void **)&buf, &buf_len);
        if (err != ERR_OK)
        {
#ifdef WS_DEBUG
            cfg->log_printf("%s: data, error = %d\n", __func__, err);
#endif
            break;
        }

        websocket_frame_header_16len_t *hdr16 = (websocket_frame_header_16len_t *)buf;
        websocket_frame_header_32len_t *hdr32 = (websocket_frame_header_32len_t *)buf;

        if (hdr32->opcode == WS_OP_CLOSE)
        {
#ifdef WS_DEBUG
            cfg->log_printf("%s: opcode = CLOSE\n", __func__);
#endif
            break;
        }

        uint32_t payload_len = hdr32->payload_len;
        char *mask = (buf + sizeof(websocket_frame_header_t));
        if (payload_len == 127)
        {
            payload_len = ntohl(hdr32->len);
            mask += FOURBYTE_LEN;
        }
        else if (payload_len == 126)
        {
            payload_len = ntohs(hdr16->len);
            mask += TWOBYTE_LEN;
        }

#ifdef WS_DEBUG
        cfg->log_printf("%s: payload len: %d\n", __func__, payload_len);
#endif

        char *payload = malloc(payload_len + 1);
        if (payload == NULL)
        {
#ifdef WS_DEBUG
            cfg->log_printf("%s: payload = malloc failed\n", __func__);
#endif
            break;
        }

        // check if content is masked
        if (hdr32->mask)
        {
            for (int i = 0; i < payload_len; i++)
            {
                payload[i] = (mask + 4)[i] ^ mask[i % 4];
            }
        }
        else
        {
            memcpy(payload, mask, payload_len);
        }

        // terminate string
        payload[payload_len] = 0;

        if ((payload != NULL) && (hdr32->opcode != WS_OP_TEXT))
        {
#ifdef WS_DEBUG
            cfg->log_printf("%s: payload AND opcode bad = %d\n", __func__, hdr32->opcode);
#endif
            free(payload);
            // I guess we only support TXT
            break;
        }

        websocket_frame_t frame;
        frame.payload_length = payload_len;
        frame.payload = payload;

        int status = cfg->callback(&frame, cfg->callback_data);
        // free payload if callback didn't free it
        if (frame.payload != NULL)
        {
            free(frame.payload);
        }
        if (status != ERR_OK)
        {
#ifdef WS_DEBUG
            cfg->log_printf("%s: callback returned: %d\n", __func__, status);
#endif
            break;
        }

        // free inbuf
        netbuf_delete(inbuf);
    }

    netbuf_delete(inbuf);
    netconn_close(conn);
    netconn_delete(conn);

    return 1;
}

static void websocket_server_task(void *p)
{
    websocket_server_config_t *cfg = (websocket_server_config_t *)p;

    websocket_conn = NULL;

    server_conn = netconn_new(NETCONN_TCP);
    if (server_conn == NULL)
    {
#ifdef WS_DEBUG
        cfg->log_printf("%s: netconn_new failed\n", __func__);
#endif
    }
    netconn_bind(server_conn, NULL, cfg->port);
    netconn_listen(server_conn);

#ifdef WS_DEBUG
    cfg->log_printf("%s: listing on port=%d\n", __func__, cfg->port);
#endif

    // set timeout, otherwise this blocks forever and we can't stop
    netconn_set_recvtimeout(server_conn, cfg->server_timeout);

    while (1)
    {
        int accepted = netconn_accept(server_conn, &websocket_conn);
        if (accepted != ERR_OK && websocket_server_running)
        {
#ifdef WS_DEBUG
            cfg->log_printf("%s: accept timeout\n", __func__);
#endif
            continue;
        }

        if (websocket_server_running == 0)
        {
            break;
        }

#ifdef WS_DEBUG
        cfg->log_printf("%s: accepted \n", __func__);
#endif

        // get client IP address
        ip_addr_t ip;
        u16_t port;
        netconn_getaddr(websocket_conn, &ip, &port, 0);
        sprintf(client_ip_address, "%s", ipaddr_ntoa(&ip));

        // set timeout, otherwise this blocks forever and we can't stop
        netconn_set_recvtimeout(websocket_conn, cfg->client_timeout);

        if (websocket_handshake(websocket_conn, cfg) != 0)
        {
#ifdef WS_DEBUG
            cfg->log_printf("%s: handshake failed\n", __func__);
#endif
            continue;
        }
#ifdef WS_DEBUG
        cfg->log_printf("%s: handshake success\n", __func__);
#endif
        conninfo_notify(cfg, 1);
        websocket_serve(websocket_conn, cfg);
        conninfo_notify(cfg, 0);
        client_ip_address[0] = 0;
        websocket_conn = NULL;
    }

    netconn_close(server_conn);
    netconn_delete(server_conn);
    websocket_server_running = 2;
    client_ip_address[0] = 0;

#ifdef WS_DEBUG
    cfg->log_printf("%s: stopped\n", __func__);
#endif

    vTaskDelete(NULL);
}

int websocket_is_connected()
{
    return websocket_conn != NULL;
}

int websocket_send(const char *data, const size_t length)
{
    if (websocket_conn == NULL)
    {
        return ERR_CONN;
    }

    uint8_t buf[sizeof(websocket_frame_header_32len_t)];
    websocket_frame_header_16len_t *hdr16 = (websocket_frame_header_16len_t *)buf;
    websocket_frame_header_16len_t *hdr32 = (websocket_frame_header_16len_t *)buf;

    hdr32->FIN = 0x1;
    size_t hdr_len = sizeof(websocket_frame_header_t);
    if (length > 0xffff)
    {
        hdr32->payload_len = 127;
        hdr32->len = ntohl(length);
        hdr_len += FOURBYTE_LEN;
    }
    else if (length > 125 && length <= 0xffff)
    {
        hdr16->payload_len = 126;
        hdr16->len = ntohs(length);
        hdr_len += TWOBYTE_LEN;
    }
    else
    {
        hdr16->payload_len = length;
    }
    hdr16->mask = 0;
    hdr16->reserved = 0;
    hdr16->opcode = WS_OP_TEXT;

    int res = netconn_write(websocket_conn, buf, hdr_len, NETCONN_COPY);

    if (res != ERR_OK)
    {
        return res;
    }

    return netconn_write(websocket_conn, data, length, NETCONN_COPY);
}

int websocket_start(websocket_server_config_t *ws_server_cfg)
{

    if (ws_server_cfg->log_printf == NULL)
    {
        ws_server_cfg->log_printf = printf;
    }

    websocket_server_running = 1;
    return xTaskCreate(&websocket_server_task, "websocket_server", 2048, ws_server_cfg, 5, NULL);
}

void websocket_stop()
{
    websocket_server_running = 0;
    if (websocket_conn != NULL)
    {
        // TODO: disconnect client if connected
    }

    // wait until task has completed
    while (websocket_server_running != 2)
    {
        vTaskDelay(100);
    }
}
