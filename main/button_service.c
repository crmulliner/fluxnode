/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"

#include "log.h"
#include "board.h"
#include "button_service.h"
#include "util.h"

struct button_service_t
{
    int button_press_count;
    unsigned long presses[BUTTON_PRESS_NUM_MAX];
    int press_idx;

    TimerHandle_t button_timer;
    int timer_started;

    int gpio;

    button_service_callback *cb;
    void *cb_data;

    unsigned long timeout_msec;
};

static struct button_service_t *button1;

static void start_timer()
{
    BaseType_t wake;
    xTimerStartFromISR(button1->button_timer, &wake);
    if (wake)
    {
        // wake timerservice
        portYIELD_FROM_ISR();
    }
}

static IRAM_ATTR void button_isr(void *pdata)
{
    if (!button1->timer_started)
    {
        button1->timer_started = 1;
        start_timer();
    }

    if (button1->press_idx < BUTTON_PRESS_NUM_MAX - 1)
    {
        button1->presses[button1->press_idx] = xTaskGetTickCountFromISR();
        button1->press_idx++;
    }
}

static void button_callback(void *pdata)
{
    button1->button_press_count = 1;
    unsigned long last = button1->presses[0];
    for (int i = 1; i < button1->press_idx; i++)
    {
        if ((button1->presses[i] - last) > BUTTON_PRESS_MIN_MS)
        {
            button1->button_press_count++;
            last = button1->presses[i];
        }
    }
    button1->press_idx = 0;

    button1->cb(button1->button_press_count, button1->timeout_msec, button1->cb_data);
    button1->button_press_count = 0;
}

void button_service_set_cb(button_service_callback cb, void *cb_data)
{
    button1->cb = cb;
    button1->cb_data = cb_data;
}

void button_server_set_timeout(unsigned long timeout_msec)
{
    button1->timeout_msec = timeout_msec;
}

void button_service_start(button_service_callback cb, void *cb_data)
{
    board_config_t *board = get_board_config();

    button1 = malloc(sizeof(struct button_service_t));
    button1->press_idx = 0;
    button1->timer_started = 0;
    button1->timeout_msec = BUTTON_SERVICE_DEFAULT_TIMEOUT;

    button_service_set_cb(cb, cb_data);

    button1->gpio = board->button_gpio;

    button1->button_timer = xTimerCreate("button1", pdMS_TO_TICKS(button1->timeout_msec), pdFALSE, NULL, button_callback);

    gpio_set_direction(button1->gpio, GPIO_MODE_INPUT);
    gpio_set_intr_type(button1->gpio, GPIO_INTR_POSEDGE);
    gpio_intr_enable(button1->gpio);

    gpio_isr_handler_add(button1->gpio, button_isr, (void *)NULL);
}

void button_service_enable(int enable)
{
    if (enable)
    {
        button1->timer_started = 0;
        button1->button_press_count = 0;
        return;
    }
    button1->timer_started = 1;
}

void button_service_stop()
{
    gpio_intr_disable(button1->gpio);
    gpio_isr_handler_remove(button1->gpio);
}

int button_service_ispressed()
{
    board_config_t *board = get_board_config();
    if (board->button_gpio != -1)
    {
        gpio_set_direction(board->button_gpio, GPIO_MODE_INPUT);
        return gpio_get_level(board->button_gpio);
    }
    return 0;
}
