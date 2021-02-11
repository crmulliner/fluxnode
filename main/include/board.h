/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#ifndef __BOARD_H__
#define __BOARD_H__

typedef uint32_t board_battery_measure_func();
typedef void board_battery_measure_toggle_func(int);
typedef void board_init_func();

typedef enum {
    BOARD_TYPE_HUZZAH = 0,
    BOARD_TYPE_FLUX,
    BOARD_TYPE_LAST
} board_type_t;

#define NUM_BOARDS BOARD_TYPE_LAST

typedef struct
{
    char boardname[16];

    int led0_gpio;
    int led1_gpio;

    int lora_cs_gpio;
    int lora_rst_gpio;

    int lora_miso_gpio;
    int lora_mosi_gpio;
    int lora_sck_gpio;

    int lora_dio0_gpio;
    int lora_dio1_gpio;
    int lora_dio2_gpio;

    int bat_adc_gpio;
    int bat_en_gpio;
    int bat_charge_gpio;

    int usb_con_gpio;

    int button_gpio;

    board_battery_measure_func *board_battery_measure;
    board_battery_measure_toggle_func *board_battery_measure_toggle;
    board_init_func *board_init;

} board_config_t;


board_type_t get_board_type();
board_config_t *get_board_config();

#endif
