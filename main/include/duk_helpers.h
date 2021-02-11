/*
 * based on: https://github.com/nkolban/duktape-esp32.git
 * license: Apache 2.0
 */

#ifndef __duk_helpers_h__
#define __duk_helpers_h__

#define ADD_FUNCTION(FUNCTION_NAME_STRING, FUNCTION_NAME, PARAM_COUNT) \
    duk_push_c_function(ctx, FUNCTION_NAME, PARAM_COUNT);              \
    duk_put_prop_string(ctx, -2, FUNCTION_NAME_STRING)

#define ADD_STRING(STR_NAME, STR_VALUE) \
    duk_push_string(ctx, STR_VALUE);    \
    duk_put_prop_string(ctx, -2, STR_NAME)

#define ADD_NUMBER(NUM_NAME, NUM_VALUE) \
    duk_push_int(ctx, NUM_VALUE);       \
    duk_put_prop_string(ctx, -2, NUM_NAME)

#endif
