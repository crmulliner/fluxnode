/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "driver/rtc_io.h"

#include "esp_spiffs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "soc/rtc.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "soc/sens_periph.h"
#include "bootloader_random.h"

#include "duk_main.h"
#include "board.h"
#include "vfs_config.h"
#include "log.h"
#include <time.h>

time_t boottime;

void app_main()
{
    board_config_t *board = get_board_config();

    esp_err_t ret = nvs_flash_init();
    ESP_ERROR_CHECK(ret);

    esp_vfs_spiffs_conf_t conf = {
        .base_path = BASE_PATH,
        .partition_label = NULL,
        .max_files = FS_MAX_NUM_FILES,
        .format_if_mount_failed = true};
    ret = esp_vfs_spiffs_register(&conf);
    ESP_ERROR_CHECK(ret);

    // general ISR service
    gpio_install_isr_service(0);

    // enable random source
    bootloader_random_enable();

    // board specific init, e.g. power managenent and wakeup
    board->board_init();

    boottime = time(0);

    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);
    // enable auto sleep
    esp_pm_config_esp32_t pm_config = {
        .max_freq_mhz = 80,
        .min_freq_mhz = 40,
        .light_sleep_enable = true};
    ESP_ERROR_CHECK(esp_pm_configure(&pm_config));

    duk_main_start();
}
