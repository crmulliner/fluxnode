#ifndef __UDP_SERVICE__
#define __UDP_SERVICE__

typedef int udp_service_cb_func(uint8_t *buf, size_t len);

struct udp_service_type
{
    int udp_srv_sock;
    int running;

    udp_service_cb_func *callback;
};

int udp_server_start(int port, udp_service_cb_func *callback;);
int udp_server_stop();

#endif
