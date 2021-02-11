#ifndef __ESP32_GLUE_H__
#define __ESP32_GLUE_H__

#include <stdio.h>
#include <sys/time.h>

duk_double_t esp32_duktape_get_now();

duk_double_t esp32_duktape_random_double();

#endif
