/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef __duk_util_h__
#define __duk_util_h__

#include <duktape.h>

void duk_util_register(duk_context *ctx);

int duk_util_run(duk_context *ctx, const char *func_str);
int duk_util_call_with_buf(duk_context *ctx, const char *func_name, const uint8_t *inbuf, const size_t len);
int duk_util_load_and_run(duk_context *ctx, const char *fname, const char *func_call);
char *duk_util_load_file(const char *fname);

#endif
