#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include <lwip/api.h>

#include "udp_service.h"

struct udp_service_type udp_server;

static void udp_server_task(void *p)
{
    struct udp_service_type *data = (struct udp_service_type *)p;

    uint8_t *buf = malloc(2048);
    while (data->running)
    {
        size_t rb = recv(data->udp_srv_sock, buf, 2048, 0);
        if (data->callback != NULL)
        {
            data->callback(buf, rb);
        }
    }
    free(buf);
}

int udp_server_start(int port, udp_service_cb_func *callback)
{
    struct sockaddr_in srv_addr;

    srv_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1)
    {
        return 0;
    }
    if (bind(sock, (struct sockaddr *)&srv_addr, sizeof(struct sockaddr_in)) == -1)
    {
        return 0;
    }

    udp_server.udp_srv_sock = sock;
    udp_server.running = 1;
    udp_server.callback = callback;

    return xTaskCreate(&udp_server_task, "udp_server_task", 2048, &udp_server, 5, NULL);
}

int udp_server_stop()
{
    udp_server.running = 0;
    close(udp_server.udp_srv_sock);
    return 1;
}
