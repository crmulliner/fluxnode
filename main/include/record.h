/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef _RECORD_H_
#define _RECORD_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

typedef int send_func(const uint8_t *buf, const size_t len, const void *data);

struct record_t
{
    size_t recordSize;
    send_func *send;
    void *send_data;
    int lenbytes;
    size_t len;
    size_t cur;
    uint8_t *buf;
};

void record_add(struct record_t *r, const uint8_t *buf, const size_t len);
size_t record_len(struct record_t *r);
uint8_t *record_get(struct record_t *r);
int record_complete(struct record_t *r);
int record_send(struct record_t *r, const uint8_t *buf, const size_t len);

#endif
