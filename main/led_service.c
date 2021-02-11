/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "soc/rtc.h"
#include "esp_pm.h"

#include "driver/uart.h"
#include "esp32/rom/uart.h"

#include "log.h"
#include "led_service.h"

static int green_led_gpio = -1;
static int red_led_gpio = -1;

static void led_cfg(int id, int hz)
{
    int gpio;
    int timer_num;
    int channel;

    switch (id)
    {
    case RED:
        gpio = red_led_gpio;
        timer_num = 1;
        channel = LEDC_CHANNEL_0;
        break;
    case GREEN:
        gpio = green_led_gpio;
        timer_num = 2;
        channel = LEDC_CHANNEL_1;
        break;
    default:
        return;
    }

    ledc_timer_config_t ledc_timer = {
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz = hz,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = timer_num,
        .clk_cfg = LEDC_USE_RTC8M_CLK,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .channel = channel,
        .duty = 0,
        .gpio_num = gpio,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .hpoint = 0,
        .timer_sel = timer_num,
    };
    ledc_channel_config(&ledc_channel);
}

static void internal_led_set(enum LED_ID_T id, int value)
{
    int channel;

    switch (id)
    {
    case RED:
        channel = LEDC_CHANNEL_0;
        break;
    case GREEN:
        channel = LEDC_CHANNEL_1;
        break;
    default:
        return;
    }
    ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, value);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}

void led_service_blink_led(enum LED_ID_T id, enum BLINK_T speed, float brightness)
{
    int hz;
    int value;

    switch (speed)
    {
    case SLOW:
        hz = 2;
        break;
    case MEDIUM:
        hz = 5;
        break;
    case FAST:
        hz = 10;
        break;
    default:
        hz = 100;
        break;
    }

    led_cfg(id, hz);

    if (brightness > 1.0 || brightness < 0.0)
    {
        brightness = 1.0;
    }

    value = 8192.0 * brightness;

#ifdef LS_DEBUG
    logprintf("%s: value = %d\n", __func__, value);
#endif
    internal_led_set(id, value);
}

void led_service_blink_led_stop(enum LED_ID_T id)
{
    ledc_stop(LEDC_LOW_SPEED_MODE, id == RED ? LEDC_CHANNEL_0 : LEDC_CHANNEL_1, 0);
}

#if 0
/*
 * pattern is 0 = both off, G = green on, R = red on, 1 = both on, '_' = 100ms pause
 * 
 * pattern = blink pattern, num = number of times to play it in a row
*/
void led_service_pattern_blink(const char *pattern, int num)
{
    gpio_pad_select_gpio(red_led_gpio);
    gpio_set_direction(red_led_gpio, GPIO_MODE_OUTPUT);
    gpio_pad_select_gpio(green_led_gpio);
    gpio_set_direction(green_led_gpio, GPIO_MODE_OUTPUT);

    int pattern_len = strlen(pattern);

    while (num > 0)
    {
        int i;
        for (i = 0; i < pattern_len; i++)
        {
            switch (pattern[i])
            {
            case '0':
                gpio_set_level(red_led_gpio, 0);
                gpio_set_level(green_led_gpio, 0);
                break;
            case 'G':
                gpio_set_level(red_led_gpio, 0);
                gpio_set_level(green_led_gpio, 1);
                break;
            case 'R':
                gpio_set_level(red_led_gpio, 1);
                gpio_set_level(green_led_gpio, 0);
                break;
            case '_':
                vTaskDelay(100 / portTICK_PERIOD_MS);
                break;
            case '1':
                gpio_set_level(red_led_gpio, 1);
                gpio_set_level(green_led_gpio, 1);
                break;
            }
        }
        num--;
    }
}
#endif

void led_set(enum LED_ID_T id, int mode)
{
    int led = red_led_gpio;
    if (id == GREEN)
    {
        led = green_led_gpio;
    }
    if (led == -1)
    {
        return;
    }
    gpio_set_level(led, mode);
}

void led_service_init(int red, int green)
{
    green_led_gpio = green;
    red_led_gpio = red;

    if (green_led_gpio != -1)
    {
        gpio_pad_select_gpio(green_led_gpio);
        gpio_set_direction(green_led_gpio, GPIO_MODE_OUTPUT);
    }

    if (red_led_gpio != -1)
    {
        gpio_pad_select_gpio(red_led_gpio);
        gpio_set_direction(red_led_gpio, GPIO_MODE_OUTPUT);
    }
}
