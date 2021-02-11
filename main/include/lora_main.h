/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef _LORA_MAIN_H_
#define _LORA_MAIN_H_

#include <time.h>

#define LORA_MSG_MAX_SIZE 255

int lora_main_register(duk_context *ctx);
int lora_main_start();

#endif
