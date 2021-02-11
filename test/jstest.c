#include <stdio.h>

#include <duktape.h>

#include "duk_util.h"

duk_context *ctx;

int main(int argc, char **argv)
{

    ctx = duk_create_heap_default();
    duk_util_register(ctx);

    duk_util_load_and_run(ctx, argv[1], "OnStart();");
}
