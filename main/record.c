/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#ifndef RECORD_TEST
#include "log.h"
#endif
#include "record.h"

void record_add(struct record_t *r, const uint8_t *buf, const size_t len)
{
    if (r->len == 0)
    {
        if (r->lenbytes == 2)
        {
            r->len = (buf[0] | (buf[1] << 8));
#ifdef RECORD_DEBUG_2
            logprintf("%s: %d\n", __func__, r->len);
#endif
        }
        r->buf = malloc(r->len);
        memcpy(r->buf, buf + 2, len - 2);
        r->cur = (len - 2);
    }
    else
    {
        if (r->len >= r->cur + len)
        {
            memcpy(r->buf + r->cur, buf, len);
            r->cur += len;
        }
    }
#ifdef RECORD_DEBUG_2
    logprintf("%s: rec cur = %d\n", __func__, r->cur);
#endif
}

size_t record_len(struct record_t *r)
{
    return r->len;
}

int record_complete(struct record_t *r)
{
    return r->len == r->cur;
}

unsigned char *record_get(struct record_t *r)
{
    uint8_t *buf = r->buf;
    r->buf = NULL;
    r->len = 0;
    r->cur = 0;
    return buf;
}

int record_send(struct record_t *r, const uint8_t *buf, const size_t len)
{
    size_t total_len = len + r->lenbytes;
    int numRecords = total_len / r->recordSize;
    if (total_len % r->recordSize > 0)
    {
        numRecords++;
    }
#ifdef RECORD_DEBUG_2
    logprintf("%s: rec %d\n", __func__, numRecords);
#endif

    int alloc_size = r->recordSize;
    if (len + 2 < alloc_size)
    {
        alloc_size = len + 2;
    }
    uint8_t *data = malloc(alloc_size);
    data[0] = (len & 0xff);
    data[1] = ((len >> 8) & 0xff);
    memcpy(&data[2], buf, alloc_size - 2);
    int res = r->send(data, alloc_size, r->send_data);
    free(data);
    if (res != 0)
    {
        return res;
    }

    size_t cur = alloc_size - 2;
    size_t cur_len = r->recordSize;

    for (int i = 1; i < numRecords; i++)
    {
        if (cur + cur_len > len)
        {
            cur_len = len - cur;
        }
#ifdef RECORD_DEBUG_2
        logprintf("%s: %d %d\n", __func__, cur, cur_len);
#endif
        int res = r->send(buf + cur, cur_len, r->send_data);
        if (res != 0)
        {
            return res;
        }
        cur += cur_len;
    }
    return 0;
}

#ifdef RECORD_TEST

#include <assert.h>

struct test_t
{
    struct record_t *r;
    char *test;
};

int snd(const uint8_t *b, const size_t len, const void *d)
{
    struct test_t *t = (struct test_t *)d;

    record_add(t->r, b, len);
    if (record_complete(t->r))
    {
        size_t l = record_len(t->r);
        uint8_t *m = record_get(t->r);
        printf("%ld '%s' expected '%s'\n", l, m, t->test);
        assert(strcmp(m, t->test) == 0);
        free(m);
    }
}

int main()
{
    struct test_t t;

    struct record_t r;
    r.lenbytes = 2;
    r.recordSize = 10;
    r.len = 0;
    r.cur = 0;
    r.buf = 0;
    r.send_data = &t;
    r.send = snd;

    t.r = &r;

    char *buf = strdup("hi hallo you what up beer? yeah?");
    t.test = buf;
    record_send(&r, buf, strlen(buf));
    buf = strdup("hi");
    t.test = buf;
    record_send(&r, buf, strlen(buf));
    buf = strdup("12345678");
    t.test = buf;
    record_send(&r, buf, strlen(buf));
    buf = strdup("1234567890");
    t.test = buf;
    record_send(&r, buf, strlen(buf));
}
#endif
