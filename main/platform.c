/*
 * Copyright: Collin Mulliner <collin AT mulliner.org>
 */

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "driver/rtc_io.h"
#include "bootloader_random.h"
#include "esp_spi_flash.h"

#include "duk_helpers.h"
#include "duk_main.h"
#include "duk_util.h"
#include "websocket.h"
#include "web_service.h"
#include "wifi_service.h"
#include "platform.h"
#include "util.h"
#include "led_service.h"
#include "button_service.h"
#include "ble_server.h"
#include "board.h"
#include "log.h"
#include "version.h"
#include "udp_service.h"

#define UDP_EVENT_SERVICE 1

extern time_t boottime;

//#define PLATFORM_DEBUG 1

/* jsondoc
{
"class": "Platform",
"longtext": "
Documentation for the Platform API.

Every application needs to contain three functions that are called by the runtime.

## OnStart()
Is called once when the application is started.
OnStart can be used to initialize your application.

## OnEvent(event)
OnEvent is called to deliver events to the application.
Events such as LoRa packet was received, a websocket/ble frame was received,
a button was pressed.

**Event Object**

The event object has a number of members.
EventType is always present. The other members are only present for
their specific event type.

```
Event = {
    EventType: uint,
    EventData: Uint8Array.plain(),
    LoRaRSSI: int,
    LoRaSNR: int,
    TimeStamp: uint,
    NumPress: uint,
}
```

**EventType (int)** identifies the type of event.
ui refer to messages received from either
the websocket connection or the BLE connection.
ui_connect/ui_disconnect refers to the UI connection
being established or torn down.

```
function EventName(event) {
    var et = ['lora', 'ui', 'ui_connected', 'ui_disconnected', 'button']; 
    return et[event.EventType];
}
```

**EventData (Plain Buffer)** contains the event payload
in the case of ui or lora events. See: https://wiki.duktape.org/howtobuffers2x

**LoRaRSSI (uint)** is set for lora events
and indicates the RSSI of the packet.

**LoRaSNR (uint)** is set for lora events
and indicates the SNR of the packet.

**TimeStamp (uint)** is set for lora events
and indicates the time of when the packet was received.

**NumPress (uint)** is set for button events
and indicates how often the button was pressed within the 3 seconds
frame after the first press.

## OnTimer()
is called after the timeout configured via Platform.setTimer() has expired.

## Example

A basic application.

```
function OnStart() {

}

function OnEvent(event) {
    // incoming websocket / BLE data
    if (event.EventType == 1) {
        var dec = new TextDecoder();
        // parse JSON object
        var cmd = JSON.parse(dec.decode(evt.EventData));
    }
}

```

## Member Variables
- BoardName (name of the board)
- Version (fluxnode software version)
- ButtonNum (number of buttons: 0-1)
- ButtonPressedDuringBoot (was button pressed during boot: 0 = no, 1 = yes)
- LEDNum (number of LEDs: 0-2)
- FlashSize (size of flash chip in bytes)

Example:
```
print('running on: ' + Platform.BoardName);
```

## Duktape/ESP32 Notes

### Random Numbers

The ESP32 provides a [hardware random number generator](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system.html#_CPPv410esp_randomv).
We use the ESP32's hw random number generator as Duktape's
random number generator. When you call `Math.random()` you get
the output of the ESP32 hw random generator.

"
}
*/

typedef enum CONNECTIVITY_T
{
    NOT_CONNECTED = 0,
    WIFI,
    BLE
} CONNECTIVITY_T;

static enum CONNECTIVITY_T connectivity;
static websocket_server_config_t *ws_server_cfg;
static board_config_t *board;
static uint8_t button_pressed_during_boot;

/* jsondoc
{
"name": "print",
"args": [
{"name": "var", "vtype": "var", "text": "multiple arguments"}],
"text": "Print to the console. Note: Print is not part of the Platform namespace.",
"example": "
print('Hi\\n');
"
}
*/

/* jsondoc
{
"name": "setSystemTime",
"args": [
{"name": "time", "vtype": "uint", "text": "time (unix epoch)"}],
"text": "Set the system time.",
"example": "
Platform.setSystemTime(1580515200);
"
}
*/
static int set_system_time(duk_context *ctx)
{
    unsigned long int newtime = duk_require_uint(ctx, 0);
    struct timeval tv;
    struct timezone tz;
    tv.tv_sec = newtime;
    tv.tv_usec = 0;
    tz.tz_minuteswest = 0;
    tz.tz_dsttime = 0;

    // adjust boottime
    if (boottime < 299000000)
    {
        boottime = newtime - boottime;
    }

    settimeofday(&tv, &tz);

#ifdef PLATFORM_DEBUG
    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    logprintf("date/time is: %s\n", strftime_buf);
#endif
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "reset",
"args": [],
"text": "
Reset the JavaScript runtime. 
This will try to load and run `main.js`. 
If `main.js` can't be run, try to run `recovery.js`. 
The idea behind `recovery.js` is that you have a fallback 
application that will allow you to recover from a error 
without power cycle and/or flashing the board.
",
"example": "
Platform.reset();
"
}
*/
static int reset(duk_context *ctx)
{
    duk_main_set_reset(1);
    return 0;
}

/* jsondoc
{
"name": "setLoadFileName",
"args": [
{"name": "filename", "vtype": "string", "text": "filename to load"}],
"text": "Set the file to be loaded the next time reset() is called.",
"example": "
// stop current application and load test.js
Platform.setLoadFileName('/test.js');
Platform.reset();
"
}
*/
static int set_load_file(duk_context *ctx)
{
    char *filename = (char *)duk_require_string(ctx, 0);
    if (filename == NULL || strlen(filename) == 0)
    {
        duk_main_set_load_file(NULL);
        return 0;
    }
    duk_main_set_load_file(strdup(filename));
    return 0;
}

/* jsondoc
{
"name": "setTimer",
"args": [{"name": "timeout", "vtype": "uint", "text": "milliseconds, 0 = disable timer"}],
"text": "Set timeout in milliseconds after which OnTimer() is called. The shortest timer is `10` milliseconds. Setting a timer of 0 will disable the timer.",
"return": "boolean status",
"example": "
// timer will go off in 5 seconds
Platform.setTimer(5000);

function OnTimer() {
    print('timer was called\\n');
}
"
}
*/
static int set_timer(duk_context *ctx)
{
    uint wake_up_time = duk_require_uint(ctx, 0);
    duk_push_boolean(ctx, duk_main_set_wake_up_time(wake_up_time));
    return 1;
}

/* jsondoc
{
"name": "getBatteryMVolt",
"args": [],
"text": "Get the battery charge level in mV.",
"return": "number",
"example": "
Platform.getBatteryMVolt();
"
}
*/
static int get_battery_mvolt(duk_context *ctx)
{
    // get battery mVolt
    duk_push_number(ctx, battery_mvolt());
    return 1;
}

/* jsondoc
{
"name": "getBatteryPercent",
"args": [],
"return": "number",
"text": "Get the battery charge level in percent.",
"example": "
Platform.getBatteryPercent();
"
}
*/
static int get_battery_percent(duk_context *ctx)
{
    // get battery percent
    duk_push_number(ctx, battery_percent());
    return 1;
}

/* jsondoc
{
"name": "getBatteryStatus",
"args": [],
"return": "number",
"text": "Get the battery charging status. 0 = draining, 1 = charging",
"example": "
if (Platform.getBatteryStatus() == 1) {
    print('battery is charging\\n');
}
"
}
*/
static int get_battery_status(duk_context *ctx)
{
    // 1 = charge 0 = drain
    duk_push_number(ctx, batt_charging());
    return 1;
}

/* jsondoc
{
"name": "getUSBStatus",
"args": [],
"return": "number",
"text": "Get the the USB connection status. 0 = disconnected, 1 = connected",
"example": "
Platform.getUSBStats();
"
}
*/
static int get_usb_connection_status(duk_context *ctx)
{
    // 1 = connected 0 = disconnected
    duk_push_number(ctx, usb_isconnected());
    return 1;
}

/* jsondoc
{
"name": "getClientConnected",
"args": [],
"return": "number",
"text": "Get the UI client status. This is either the websocket or the BLE connection. 0 = disconnected, 1 = connected",
"example": "
if (Platform.getClientConnected() == 1) {
    print('client is connected\\n');
}
"
}
*/
static int get_client_connected(duk_context *ctx)
{
    int con = 0;
    if (connectivity == WIFI)
    {
        con = websocket_is_connected();
    }
    else if (connectivity == BLE)
    {
        con = ble_server_is_connected();
    }
    duk_push_number(ctx, con);
    return 1;
}

static int client_disconnect(duk_context *ctx)
{
#ifdef PLATFORM_DEBUG
    logprintf("%s: not implemented\n", __func__);
#endif
    // websocket does not support this yet
    // 1 = was connected 0 = was disconnected
    duk_push_number(ctx, 0);
    return 1;
}

/* jsondoc
{
"name": "getLocalIP",
"args": [],
"return": "string",
"text": "Get the local IP of the board. If connectivity is Wifi.",
"example": "
print('my IP is: ' + Platform.getLocalIP() + '\\n');
"
}
*/
static int get_local_ip(duk_context *ctx)
{
    char *local_ip = wifi_service_get_ip();

    if (strlen(local_ip) == 0)
    {
        local_ip = "none";
    }

    duk_push_string(ctx, local_ip);
    return 1;
}

/* jsondoc
{
"name": "getClientID",
"args": [],
"return": "string",
"text": "Get the ID of the client. Either an IP address or a BLE MAC, 'none' no client connected.",
"example": "
print('client ID is: ' + Platform.getClientID() + '\\n');
"
}
*/
static int get_client_id(duk_context *ctx)
{
    char *client = "none";

    if (connectivity == WIFI)
    {
        client = websocket_get_client_ip();
    }
    else if (connectivity == BLE)
    {
        client = ble_server_get_client_addr();
    }
    duk_push_string(ctx, client);
    return 1;
}

/* jsondoc
{
"name": "getConnectivity",
"args": [],
"return": "number",
"text": "Get the connectivity mode. 0 = wifi/ble off, 1 = wifi, 2 = BLE.",
"example": "
if (Platform.getConnectivity() == 0) {
    print('disconnected from the world\\n');
}
"
}
*/
static int get_connectivity(duk_context *ctx)
{
    duk_push_number(ctx, connectivity);
    return 1;
}

static int wifi_connectivity_stop()
{
    websocket_stop();
    webserver_stop();
#ifdef UDP_EVENT_SERVICE
    udp_server_stop();
#endif
    if (ws_server_cfg != NULL)
    {
        free(ws_server_cfg);
        ws_server_cfg = NULL;
    }

    wifi_stop();

    return 1;
}

static int ble_receive(const uint8_t *buf, const size_t len)
{
    duk_main_add_event(UI_MSG, INCOMING, (uint8_t *)buf, len);
    return 0;
}

static void ui_conninfo(int status)
{
    int conn_status = UI_DISCONNECTED;
    switch (status)
    {
    case 0:
        conn_status = UI_DISCONNECTED;
        break;
    case 1:
        conn_status = UI_CONNECTED;
        break;
    }
    duk_main_add_event(conn_status, INCOMING, NULL, 0);
}

/* jsondoc
{
"name": "setConnectivity",
"args": [{"name": "con", "vtype": "uint", "text": "connectivity mode"}],
"text": "Set connectivity. 0 = off, 1 = wifi, 2 = BLE.",
"return": "boolean status",
"example": "
// switch to BLE
Platform.setConnectivity(2);
"
}
*/
static int set_connectivity(duk_context *ctx)
{
    double newcon = duk_require_int(ctx, 0);

    if (connectivity == NOT_CONNECTED)
    {
        bootloader_random_disable();
    }

    if (newcon == NOT_CONNECTED)
    {
        if (connectivity == WIFI)
        {
            wifi_connectivity_stop();
        }
        else if (connectivity == BLE)
        {
            ble_server_stop();
        }
        bootloader_random_enable();
        duk_main_set_send_func(NULL);
    }
    else if (newcon == WIFI)
    {
        if (connectivity == BLE)
        {
            ble_server_stop();
        }
        duk_main_set_send_func(NULL);
    }
    else if (newcon == BLE)
    {
        if (connectivity == WIFI)
        {
            wifi_connectivity_stop();
        }
        ble_server_set_receive_callback(ble_receive);
        ble_server_set_battery_percent(battery_percent);
        ble_server_set_conninfo_callback(ui_conninfo);
        ble_server_start();
        duk_main_set_send_func(ble_server_send);
    }
    connectivity = newcon;
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "bleSetPasscode",
"args": [{"name": "pass", "vtype": "uint", "text": "BLE pin"}],
"return": "boolean status",
"text": "Set BLE passcode.",
"example": "
Platform.bleSetPasscode(1234);
"
}
*/
static int ble_set_passcode(duk_context *ctx)
{
    unsigned int pk = duk_require_uint(ctx, 0);
    ble_server_set_passkey(pk);
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "bleBondAllow",
"args": [{"name": "bond", "vtype": "uint", "text": "allow = 1, disallow = 0"}],
"return": "boolean status",
"text": "Set BLE bond mode.",
"example": "
// allow the next BLE bonding attempt
Platform.bleBondAllow(1);
"
}
*/
static int ble_allow_bond(duk_context *ctx)
{
    unsigned int allow = duk_require_uint(ctx, 0);
    ble_server_accept_bonding(allow);
    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "bleBondRemove",
"args": [{"name": "baddr", "vtype": "string", "text": "BLE MAC to remove"}],
"text": "Remove bonding for a BLE MAC.",
"return": "boolean status",
"example": "
Platform.bleBondRemove('00:11:22:33:44:55');
"
}
*/
static int ble_remove_bond(duk_context *ctx)
{
    char *bda_out = (char *)duk_require_string(ctx, 0);
    char *bda = strdup(bda_out);
    if (connectivity == BLE)
    {
        esp_bd_addr_t bd;
        if (strlen(bda) != 17)
        {
            free(bda);
            duk_push_boolean(ctx, 0);
            return 1;
        }
        int p = 0;
        for (int i = 2; i < 18; i += 3)
        {
            bda[i] = 0;
            bd[p] = strtol(&bda[i - 2], NULL, 16);
            p++;
        }
        free(bda);
        ble_support_remove_bonded_device(&bd);
        duk_push_boolean(ctx, 1);
    }
    else
    {
        free(bda);
        duk_push_boolean(ctx, 0);
    }
    return 1;
}

/* jsondoc
{
"name": "bleBondGetDevices",
"args": [],
"return": "array of strings",
"text": "Get BLE addresses that are currently bonded. This returns a string array.",
"example": "
var devs = Platform.bleBondGetDevices();
print(devs);
"
}
*/
static int ble_get_bonded(duk_context *ctx)
{
    esp_ble_bond_dev_t *dev_list = NULL;
    int num_devs = 0;
    ble_support_get_bonded_devices(&dev_list, &num_devs);
    duk_idx_t arr_idx = duk_push_array(ctx);
    for (int i = 0; i < num_devs; i++)
    {
        char bd[24];
        sprintf(bd, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
                dev_list[i].bd_addr[0],
                dev_list[i].bd_addr[1],
                dev_list[i].bd_addr[2],
                dev_list[i].bd_addr[3],
                dev_list[i].bd_addr[4],
                dev_list[i].bd_addr[5]);
        duk_push_string(ctx, bd);
        duk_put_prop_index(ctx, arr_idx, i);
    }
    if (dev_list)
        free(dev_list);
    return 1;
}

/* jsondoc
{
"name": "bleGetDeviceAddr",
"args": [],
"return": "string BLE address",
"text": "Get BLE local device address",
"example": "
var dev = Platform.bleGetDeviceAddr();
"
}
*/
static int ble_get_device_addr(duk_context *ctx)
{
    uint8_t addr[6] = {0};
    char bd[24];
    esp_read_mac(addr, ESP_MAC_BT);
    sprintf(bd, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
            addr[0],
            addr[1],
            addr[2],
            addr[3],
            addr[4],
            addr[5]);
    duk_push_string(ctx, bd);
    return 1;
}

/* jsondoc
{
"name": "wifiGetMacAddr",
"args": [],
"return": "string MAC address",
"text": "Get WIFI MAC address",
"example": "
var dev = Platform.wifiGetMacAddr();
"
}
*/
static int wifi_get_mac_addr(duk_context *ctx)
{
    uint8_t addr[6] = {0};
    char bd[24];
    esp_read_mac(addr, ESP_MAC_WIFI_STA);
    sprintf(bd, "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x",
            addr[0],
            addr[1],
            addr[2],
            addr[3],
            addr[4],
            addr[5]);
    duk_push_string(ctx, bd);
    return 1;
}

static int queue_cb(websocket_frame_t *frame, const void *data)
{
    duk_main_add_event(UI_MSG, INCOMING, (uint8_t *)frame->payload, frame->payload_length);
    // signal websocket_server that we handle free(payload)
    frame->payload = NULL;
    return 0;
}

#ifdef UDP_EVENT_SERVICE
static int queue_udp_cb(const uint8_t *buf, const size_t len)
{
    if (len < 2)
    {
        return 0;
    }

    uint8_t *data = malloc(len - 1);
    memcpy(data, buf + 1, len - 1);

    // first byte in UDP packet selects the event
    uint8_t msg_type = buf[0] - 'A';

    duk_main_add_full_event(msg_type, INCOMING, (uint8_t *)data, len - 1, 0, 0, time(NULL));
    return 0;
}
#endif

/* jsondoc
{
"name": "wifiConfigure",
"args": [
{"name": "ssid", "vtype": "string", "text": "WIFI SSID"},
{"name": "password", "vtype": "string", "text": "WPA password (min 8 characters)"},
{"name": "mode", "vtype": "uint", "text": "0 = AP, 1 = client"},
{"name": "web_mode", "vtype": "uint", "text": "webserver API readonly = 1, read/write = 0"}
],
"text": "Configure Wifi. The web_mode can be used to lock down the web API.",
"return": "boolean status",
"example": "
Platform.setConnectivity(1);
// start WIFI AP and set web mode to R/W
var success = Platform.wifiConfigure('fluxnode','fluxulf', 0, 1);
if (success == false) {
    print('wifi start failed, check SSID and password\\n');
}
"
}
*/
static int wifi_configure(duk_context *ctx)
{
    char *ssid = (char *)duk_require_string(ctx, 0);
    char *pass = (char *)duk_require_string(ctx, 1);
    int mode = duk_require_int(ctx, 2);
    int webmode = duk_require_int(ctx, 3);

    if (strlen(pass) > 0 && strlen(pass) < 8)
    {
        connectivity = NOT_CONNECTED;
        duk_push_boolean(ctx, 0);
        return 1;
    }

    // mode 0 = AP 1 = Client
    int res = wifi_start(mode == 0 ? wifi_mode_ap : wifi_mode_sta, ssid, pass);

    if (res == 0)
    {
        connectivity = NOT_CONNECTED;
        duk_push_boolean(ctx, 0);
        return 1;
    }

    ws_server_cfg = malloc(sizeof(websocket_server_config_t));
    ws_server_cfg->port = 8888;
    ws_server_cfg->server_timeout = 5000;
    ws_server_cfg->client_timeout = 60000;
    ws_server_cfg->callback_data = NULL;
    ws_server_cfg->callback = queue_cb;
    ws_server_cfg->log_printf = logprintf;
    ws_server_cfg->conninfo_callback = ui_conninfo;

    duk_main_set_send_func(websocket_send);

    websocket_start(ws_server_cfg);
    webserver_start(webmode);

#ifdef UDP_EVENT_SERVICE
    udp_server_start(5000, queue_udp_cb);
#endif

    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "sendEvent",
"args": [
{"name": "event_type", "vtype": "uint", "text": "1 = UI event (send to websocket or BLE)"},
{"name": "data", "vtype": "plain buffer", "text": "data to send (see: https://wiki.duktape.org/howtobuffers2x)"}
],
"text": "Send event to the connected UI client. Only UI event (1) is currently supported.",
"return": "boolean status",
"example": "
var x = {
    test: 'test string',
    num: 1337,
};
Platform.sendEvent(1, Uint8Array.allocPlain(JSON.stringify(x)));
"
}
*/
static int send_event(duk_context *ctx)
{
    // event_type should always be UI event
    int event_type = duk_require_int(ctx, 0);
    duk_size_t buff_len = 0;
    void *buff_ptr = duk_require_buffer(ctx, 1, &buff_len);
    // duplicate buffer
    uint8_t *buff = malloc(buff_len);
    memcpy(buff, (uint8_t *)buff_ptr, buff_len);
    duk_main_add_event(event_type, OUTGOING, buff, buff_len);

    duk_push_boolean(ctx, 1);
    return 1;
}

/* jsondoc
{
"name": "reboot",
"args": [],
"text": "Reboot the board.",
"example": "
Platform.reboot();
"
}
*/
static int reboot(duk_context *ctx)
{
    // should we do anyting before calling restart?
    esp_restart();
    return 0;
}

/* jsondoc
{
"name": "setLED",
"args": [
{"name": "led_id", "vtype": "uint", "text": "LED id (0-1)"},
{"name": "onoff", "vtype": "uint", "text": "on (1) / off (0) "}
],
"text": "Set an LED on/off.",
"example": "
Platform.setLED(0, 1);
"
}
*/
static int led_set_led(duk_context *ctx)
{
    int led_id = duk_require_int(ctx, 0);
    int onoff = duk_require_int(ctx, 1);

    if ((led_id == 0 && board->led0_gpio == -1) || (led_id == 1 && board->led1_gpio == -1))
    {
        return 0;
    }
    led_set(led_id, onoff);
    return 0;
}

/* jsondoc
{
"name": "setLEDBlink",
"args": [
{"name": "led_id", "vtype": "uint", "text": "LED id (0-1)"},
{"name": "rate", "vtype": "uint", "text": "0-4"},
{"name": "brightness", "vtype": "double", "text": "0.0-1.0"}
],
"text": "Set an LED to blink at a given rate and brightness.",
"example": "
Platform.setLEDBlink(0, 2, 0.5);
"
}
*/
static int led_blink_start(duk_context *ctx)
{
    int led_id = duk_require_int(ctx, 0);
    int speed = duk_require_int(ctx, 1);
    double level = duk_require_number(ctx, 2);

    if ((led_id == 0 && board->led0_gpio == -1) || (led_id == 1 && board->led1_gpio == -1))
    {
        return 0;
    }
    led_service_blink_led(led_id, speed, level);
    return 0;
}

/* jsondoc
{
"name": "stopLEDBlink",
"args": [{"name": "led_id", "vtype": "uint", "text": "LED id (0-1)"}],
"text": "Stop an LED blinking.",
"example": "
Platform.stopLEDBlink(0);
"
}
*/
static int led_blink_stop(duk_context *ctx)
{
    int led_id = duk_require_int(ctx, 0);

    if ((led_id == 0 && board->led0_gpio == -1) || (led_id == 1 && board->led1_gpio == -1))
    {
        return 0;
    }
    led_service_blink_led_stop(led_id);
    return 0;
}

/* jsondoc
{
"name": "isButtonPressed",
"args": [],
"return": "boolean status",
"text": "Returns True if the button is pressed while this function is called.",
"example": "
if (Platform.isButtonPressed()) {
    print('somebody is pressing the button right now');
}
"
}
*/
static int is_button_pressed(duk_context *ctx)
{
    int button_state = button_service_ispressed();
    duk_push_boolean(ctx, button_state);
    return 1;
}

/* jsondoc
{
"name": "getFreeHeap",
"args": [],
"return": "number",
"text": "Returns number of free bytes on the heap.",
"example": "
print('we got ' + Platform.getFreeHeap()/1024 + 'K free heap');
"
}
*/
static int heap_free(duk_context *ctx)
{
    unsigned int heap_free_size = esp_get_free_heap_size();
    duk_push_number(ctx, heap_free_size);
    return 1;
}

/* jsondoc
{
  "name": "getFreeInternalHeap",
  "args": [
  ],
  "return": "number",
  "text": "Returns number of free bytes on the internal heap.",
  "example": "
print('we got ' + Platform.getFreeInternalHeap()/1024 + 'K free internal heap');
"
}
*/
static int heap_internal_free(duk_context *ctx)
{
    unsigned int heap_free_size = esp_get_free_internal_heap_size();
    duk_push_number(ctx, heap_free_size);
    return 1;
}

/* jsondoc
{
"name": "gpioWrite",
"args": [
{"name": "gpionum", "vtype": "uint", "text": "GPIO number"},
{"name": "value", "vtype": "uint", "text": "0 or 1"}
],
"text": "Write to GPIO. This also configures the given GPIO as an output GPIO. Use with caution!",
"example": "
Platform.gpioWrite(15, 1);
"
}
*/
static int gpio_output(duk_context *ctx)
{
    int gpio = duk_require_int(ctx, 0);
    int level = duk_require_int(ctx, 1);
    gpio_pad_select_gpio(gpio);
    gpio_set_direction(gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(gpio, level);
    return 0;
}

/* jsondoc
{
"name": "gpioRead",
"args": [{"name": "gpionum", "vtype": "uint", "text": "GPIO number"}],
"return": "number",
"text": "Read from GPIO. This also configures the given GPIO as an input GPIO. Use with caution!",
"example": "
Platform.gpioRead(14);
"
}
*/
static int gpio_input(duk_context *ctx)
{
    int gpio = duk_require_int(ctx, 0);
    gpio_pad_select_gpio(gpio);
    gpio_set_direction(gpio, GPIO_MODE_INPUT);
    duk_push_number(ctx, gpio_get_level(gpio));
    return 1;
}

/* jsondoc
{
"name": "loadLibrary",
"args": [{"name": "filename", "vtype": "string", "text": "filename to load"}],
"text": "Load JavaScript code into current application.",
"return": "boolean status",
"example": "
Platform.loadLibrary('/timer.js');
"
}
*/
static int load_library(duk_context *ctx)
{
    char *lib = (char *)duk_require_string(ctx, 0);
    if (lib == NULL)
    {
        duk_push_boolean(ctx, 0);
        return 1;
    }

    int res = duk_util_load_and_run(ctx, lib, NULL);

    duk_push_boolean(ctx, res == 1);
    return 1;
}

/* jsondoc
{
"name": "getBootime",
"args": [],
"text": "get boot time",
"return": "time of system boot up",
"example": "
"
}
*/
static int get_boottime(duk_context *ctx)
{
    duk_push_uint(ctx, boottime);
    return 1;
}

static duk_function_list_entry platform_funcs[] = {
    {"getFreeHeap", heap_free, 0},
    {"getFreeInternalHeap", heap_internal_free, 0},
    {"gpioWrite", gpio_output, 2},
    {"gpioRead", gpio_input, 1},
    {"isButtonPressed", is_button_pressed, 0},
    {"setSystemTime", set_system_time, 1},
    {"getBatteryStatus", get_battery_status, 0},
    {"getBatteryMVolt", get_battery_mvolt, 0},
    {"getBatteryPercent", get_battery_percent, 0},
    {"getUSBStatus", get_usb_connection_status, 0},
    {"wifiConfigure", wifi_configure, 4},
    {"wifiGetMacAddr", wifi_get_mac_addr, 0},
    {"bleSetPasscode", ble_set_passcode, 1},
    {"bleBondRemove", ble_remove_bond, 1},
    {"bleBondAllow", ble_allow_bond, 1},
    {"bleBondGetDevices", ble_get_bonded, 0},
    {"bleGetDeviceAddr", ble_get_device_addr, 0},
    {"setConnectivity", set_connectivity, 1},
    {"getConnectivity", get_connectivity, 0},
    {"getClientConnected", get_client_connected, 0},
    {"clientDisconnect", client_disconnect, 0},
    {"getClientID", get_client_id, 0},
    {"getLocalIP", get_local_ip, 0},
    {"setLED", led_set_led, 2},
    {"setLEDBlink", led_blink_start, 3},
    {"stopLEDBlink", led_blink_stop, 1},
    {"reset", reset, 0},
    {"reboot", reboot, 0},
    {"getBoottime", get_boottime, 0},
    {"setLoadFileName", set_load_file, 1},
    {"setTimer", set_timer, 1},
    {"sendEvent", send_event, 2},
    {"loadLibrary", load_library, 1},
    {NULL, NULL, 0},
};

void platform_register(duk_context *ctx)
{
    duk_push_global_object(ctx);
    duk_push_object(ctx);

    ADD_STRING("BoardName", board->boardname);
    ADD_STRING("Version", FLUXNODE_VERSION);
    ADD_NUMBER("ButtonNum", board->button_gpio != -1 ? 1 : 0);
    ADD_NUMBER("LEDNum", board->led1_gpio != -1 ? 2 : 1);
    ADD_NUMBER("FlashSize", spi_flash_get_chip_size());
    ADD_NUMBER("ButtonPressedDuringBoot", button_pressed_during_boot);

    // will configure and reset LEDs (e.g. if on or blinking before)
    led_service_init(board->led0_gpio, board->led1_gpio);

    duk_put_function_list(ctx, -1, platform_funcs);
    duk_put_prop_string(ctx, -2, "Platform");
    duk_pop(ctx);
}

void platform_init()
{
    // can be used for factory reset
    button_pressed_during_boot = button_service_ispressed();

    connectivity = NOT_CONNECTED;
    board = get_board_config();
}
