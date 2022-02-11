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
#include "soc/rtc.h"
#include "esp_pm.h"
#include "esp_sleep.h"

#include "log.h"
#include "util.h"
#include "board.h"

//#define UTIL_DEBUG_2 1

static void flux_batt_en_toggle(int on);
static uint32_t flux_battery_measure_mvolt();
static void flux_board_init();

static uint32_t huzzah_battery_measure_mvolt();
static void huzzah_board_init();

static board_config_t boards[NUM_BOARDS] = {
    {
        "huzzah",
        // led 0 1
        13,
        -1,
        // cs, rst
        33,
        27,
        // miso, mosi, sck
        19,
        18,
        5,
        // dio 0 1 2
        32,
        15,
        14,
        // bat
        -1,
        -1,
        -1,
        // usb con
        -1,
        // button
        -1,
        huzzah_battery_measure_mvolt,
        NULL,
        huzzah_board_init,
    },
    {
        "fluxn0de",
        // led 0 1
        27,
        15,
        // cs, rst,
        21,
        4,
        // miso, mosi, sck
        19,
        18,
        5,
        // dio 0 1 2
        34,
        39,
        36,
        // bat
        -1,
        26,
        2,
        // usb con
        13,
        // button
        14,
        flux_battery_measure_mvolt,
        flux_batt_en_toggle,
        flux_board_init,
    }};

static board_config_t *this_board = NULL;
static board_type_t board_type = -1;

static uint32_t huzzah_battery_measure_mvolt()
{
    esp_adc_cal_characteristics_t *adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, adc_chars);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_11);

    uint32_t val = adc1_get_raw(ADC1_CHANNEL_7);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(val, adc_chars);

    // huzzah esp32 has a 2x divider between voltage PIN and GPIO
    voltage = voltage * 2;
#ifdef UTIL_DEBUG_2
    logprintf("%s: %d mV\n", __func__, voltage);
#endif
    return voltage;
}

static void huzzah_board_init()
{
    // disable external clock otherwise SPI doesn't work
    rtc_clk_32k_enable(false);
    // 32 is LoRa IRQ
    esp_sleep_enable_ext1_wakeup(GPIO_SEL_32, ESP_EXT1_WAKEUP_ANY_HIGH);
}

static void flux_board_init()
{
    // 34 is LoRa IRQ
    // 14 is button
    esp_sleep_enable_ext1_wakeup(GPIO_SEL_34 | GPIO_SEL_14, ESP_EXT1_WAKEUP_ANY_HIGH);
}

static void flux_batt_en_toggle(int on)
{
    gpio_pad_select_gpio(boards[BOARD_TYPE_FLUX].bat_en_gpio);
    gpio_set_direction(boards[BOARD_TYPE_FLUX].bat_en_gpio, GPIO_MODE_OUTPUT);
    if (on)
    {
        adc_power_on();
        gpio_set_level(boards[BOARD_TYPE_FLUX].bat_en_gpio, 1);
    }
    else
    {
        gpio_set_level(boards[BOARD_TYPE_FLUX].bat_en_gpio, 0);
        adc_power_off();
    }
}

static uint32_t flux_battery_measure_mvolt_internal(uint32_t *valout)
{
    esp_adc_cal_characteristics_t *adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_0, ADC_WIDTH_BIT_12, 1100, adc_chars);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_0);

    uint32_t val = adc1_get_raw(ADC1_CHANNEL_7);
    uint32_t voltage = esp_adc_cal_raw_to_voltage(val, adc_chars);

    if (valout)
    {
        *valout = val;
    }

    // fluxn0de has a 11x divider on the voltage
#ifdef UTIL_DEBUG_2
    logprintf("%s: %d %d mV\n", __func__, val, voltage * 11);
#endif
    return voltage * 11;
}

static uint32_t flux_battery_measure_mvolt()
{
    return flux_battery_measure_mvolt_internal(NULL);
}

// --- board detection ---

#define BATT_EN_GPIO 26
#define CHARGE_USB_GPIO 2
#define USB_CON_GPIO 13

/*
 * This function tries to detect the board it is running on.
 * Right now detection is based on battery volt measurement.
 * The fluxboard has an enable GPIO that needs to be set to
 * 1 in order to feed the battery voltage to the ADC. Without it
 * the fluxboard will read 0 volt while the HUZZAH board will
 * have some value other than 0.
 * 
 */
static board_type_t board_detect()
{
    gpio_pad_select_gpio(BATT_EN_GPIO);
    gpio_set_direction(BATT_EN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BATT_EN_GPIO, 0);
    adc_power_on();
    uint32_t val;
    flux_battery_measure_mvolt_internal(&val);
    adc_power_off();
    if (val == 0)
    {
        board_type = BOARD_TYPE_FLUX;
        flux_batt_en_toggle(0);
        return board_type;
    }
    board_type = BOARD_TYPE_HUZZAH;
    return board_type;
}

// END --- board detection ---

board_type_t get_board_type()
{
    if (board_type != -1)
    {
        return board_type;
    }
    board_type = board_detect();
    return board_type;
}

board_config_t *get_board_config()
{
    if (this_board)
    {
        return this_board;
    }
    this_board = &boards[get_board_type()];
    return this_board;
}
