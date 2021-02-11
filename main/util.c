/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include <driver/adc.h>
#include "esp_adc_cal.h"

#include "log.h"
#include "util.h"
#include "board.h"

#define BATTERY_VOLT_MIN 3300.0
#define BATTERY_VOLT_MAX 4200.0
#define BATTERY_MEASURE_NUM 10
float battery_mvolt()
{
    board_config_t *board = get_board_config();

    if (board->board_battery_measure == NULL)
    {
        return 0;
    }

    if (board->board_battery_measure_toggle)
    {
        board->board_battery_measure_toggle(1);
    }

    float mvolt = 0;
    for (int i = 0; i < BATTERY_MEASURE_NUM; i++)
    {
        mvolt += (float)board->board_battery_measure();
    }

    if (board->board_battery_measure_toggle)
    {
        board->board_battery_measure_toggle(0);
    }
    mvolt = mvolt / (float)BATTERY_MEASURE_NUM;
    return mvolt;
}

int battery_percent()
{
    float mvolt = battery_mvolt();
#ifdef UTIL_DEBUG_1
    logprintf("%s: %d mV\n", __func__, (int)mvolt);
#endif
    float volt = (mvolt - BATTERY_VOLT_MIN) / (BATTERY_VOLT_MAX - BATTERY_VOLT_MIN);
    volt = volt * 100.0;
    return volt > 100 ? 100 : volt;
}

int batt_charging()
{
    board_config_t *board = get_board_config();

    if (board->bat_charge_gpio == -1)
    {
        return 0;
    }
    gpio_pad_select_gpio(board->bat_charge_gpio);
    gpio_set_direction(board->bat_charge_gpio, GPIO_MODE_INPUT);
    return !gpio_get_level(board->bat_charge_gpio);
}

int usb_isconnected()
{
    board_config_t *board = get_board_config();

    if (board->usb_con_gpio == -1)
    {
        return 0;
    }
    gpio_pad_select_gpio(board->usb_con_gpio);
    gpio_set_direction(board->usb_con_gpio, GPIO_MODE_INPUT);
    return gpio_get_level(board->usb_con_gpio);
}

void util_init()
{
    // configure GPIOs
}
