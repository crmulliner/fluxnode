idf_component_register(SRCS
    "main.c"
    "duk_util.c"
    "wifi_service.c"
    "duk_fs.c"
    "duk_main.c"
    "web_service.c"
    "util.c"
    "platform.c"
    "led_service.c"
    "lora_main.c"
    "button_service.c"
    "ble_server.c"
    "ble_support.c"
    "record.c"
    "duk_crypto.c"
    "board.c"
    "udp_service.c"
    INCLUDE_DIRS 
        "include"
        "."
)

spiffs_create_partition_image(storage ../spiffs_image FLASH_IN_PROJECT)
