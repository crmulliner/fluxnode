/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>

#include <duktape.h>

#include "duk_helpers.h"
#include "log.h"

#define DUK_UTIL_DEBUG 1

static duk_ret_t native_print(duk_context *ctx)
{
    duk_push_string(ctx, " ");
    duk_insert(ctx, 0);
    duk_join(ctx, duk_get_top(ctx) - 1);
    printf("%s", duk_safe_to_string(ctx, -1));
    return 0;
}

void duk_util_register(duk_context *ctx)
{
    duk_push_c_function(ctx, native_print, DUK_VARARGS);
    duk_put_global_string(ctx, "print");
}

int duk_util_run(duk_context *ctx, const char *func_str)
{
    int result = 1;
    if (duk_peval_string(ctx, func_str) != 0)
    {
#ifdef DUK_UTIL_DEBUG
        logprintf("%s: eval failed: %s\n", __func__, duk_safe_to_string(ctx, -1));
#endif
        result = 0;
    }
#if 0
    else
    {
        logprintf("%s: result is: %s\n", __func__, duk_get_string(ctx, -1));
    }
#endif
    duk_pop(ctx);
    return result;
}

int duk_util_call_with_buf(duk_context *ctx, const char *func_name, const uint8_t *inbuf, const size_t len)
{
    int result = 1;
    duk_push_global_object(ctx);
    duk_get_prop_string(ctx, -1, func_name);
    uint8_t *buf = (uint8_t *)duk_push_fixed_buffer(ctx, len);
    memcpy(buf, inbuf, len);
    duk_insert(ctx, -1);
    if (duk_pcall(ctx, 1 /*nargs*/) != 0)
    {
        result = 0;
#ifdef DUK_UTIL_DEBUG
        logprintf("%s: eval failed: %s\n", __func__, duk_safe_to_string(ctx, -1));
#endif
    }
    duk_pop(ctx);
    return result;
}

char *duk_util_load_file(const char *fname)
{
    FILE *fp = fopen(fname, "r");
    if (fp == NULL)
    {
        logprintf("%s: can't open: %s\n", __func__, fname);
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long int fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buff = malloc(fsize + 1);
    if (buff == NULL)
    {
        fclose(fp);
#ifdef DUK_UTIL_DEBUG
        logprintf("%s: can't allocate %ld bytes\n", __func__, fsize);
#endif
        return NULL;
    }
    int num = fread(buff, 1, fsize, fp);
    fclose(fp);
    buff[fsize - 1] = 0;
    if (num < fsize)
    {
#ifdef DUK_UTIL_DEBUG
        logprintf("%s: only read %d out of %ld bytes\n", __func__, num, fsize);
#endif
        free(buff);
        return NULL;
    }
    return buff;
}

int duk_util_load_and_run(duk_context *ctx, const char *fname, const char *func_call)
{
    char *buff = duk_util_load_file(fname);
    if (buff == NULL)
    {
        return 0;
    }
    int result = duk_util_run(ctx, buff);
    free(buff);
    if (!result)
        return result;
    if (func_call)
        return duk_util_run(ctx, func_call);
    return 1;
}
