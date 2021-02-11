/*
 * Copyright: Collin Mulliner
 */

#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "driver/rtc_io.h"

#include <duktape.h>

duk_double_t esp32_duktape_get_now()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	duk_double_t ret = floor((double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0);
	return ret;
}

duk_double_t esp32_duktape_random_double()
{
	uint32_t r = esp_random();
	duk_double_t rnd = r;
	return rnd;
}
