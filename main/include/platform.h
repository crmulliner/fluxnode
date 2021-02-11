/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef __platform_h__
#define __platform_h__

#include <duktape.h>

void platform_register(duk_context *ctx);
void platform_init();

#endif
