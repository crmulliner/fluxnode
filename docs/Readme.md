# Fluxn0de

Fluxn0de is:
- a tool to explore and prototype LoRa and LoRaWAN applications
- designed to run on battery powered devices
- designed to run on [ESP32 based boards](#boards)
- running JavaScript application (Fluxn0de uses the [Duktape](https://duktape.org/) runtime)
- designed with web application in mind as the main interface (http server + websocket server implementation)
- [BLE](BLE.md) service that (kind of) emulates a websocket connection
- BLE battery level service

## JavaScript API
- [Platform](platform.md) Control Wifi, BLE, LEDs, JavaScript runtime,...
- [LoRa](lora.md) LoRa modem API
- [Crypto](crypto.md) Crypto API (tailored towards LoRaWAN)
- [FileSystem](filesystem.md) Access files on the flash filesystem

## HTTP API
- [WebService](webservice.md) HTTP API to interact with the webserver (if Wifi is enabled)

## BLE
- [BLE](BLE.md) Fluxn0de BLE services

## JavaScript Libraries
- [Timer](timer.md) Timer library that provides basic timer functionality
- [LoRaWan](lorawanlib.md) Basic LoRaWAN library

## Boards
- [Adafruit Huzzah](huzzah.md)
- [Fluxboard](fluxboard.md)

## - Getting Started -

Ideally you have build ESP32 software before and are familiar with the ESP32 SDK
and tools such as `idf.py`.

### Config

Before you flash your Fluxn0de configure the wifi parameters
in [recovery.js](../spiffs_image/recovery.js) and [test.js](../spiffs_image/test.js).
If you forget to do that you can always connect to the Wifi network that is automatically created by Fluxn0de.
The recovery SSID is `fluxn0de` and the password is `fluxn0de`. Recovery is indicated by a blinking red LED.
`192.168.4.1` is the default IP for Fluxn0de in Wifi AP mode.

### Building & Flashing

Setup:
- [Install ESP32 SDK](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html)
- check that `idf.py` is in your path

Build:
- `make`

Flash (and connect serial console):
- `make flash`

Connect serial console (and reboot):
- `make monitor`

### Examples

All examples use the [HTTP API](webservice.md) to control the Fluxn0de from a computer. Make sure to take a look.
Check that you have `curl` installed (all examples rely on curl)

There are a number of examples applications that are installed by default. From easiest to hardest:

- [test.js](examples/test.md) basic demo app
- [Crypto Test](examples/cryptotest.md) crypto API tests
- [LoRaWanScan](examples/lorawanscan.md) LoRaWAN demo app
- [BLE echo server](examples/ble_echo_server.md) BLE echo server
- [FileSystem Test](examples/filesystem.md) FileSystem API tests

### Advanced

The two JavaScript applications [main.js](../spiffs_image/main.js) and [recovery.js](../spiffs_image/recovery.js)
have special meaning for Fluxn0de. `main.js` is always loaded on startup / reset of the JavaScript runtime.
The idea behind this is that you can load a different application from `main.js` or overwrite `main.js` with your application
to have it start on reset. `recovery.js` is a fallback for when an error crashes the current application. `recovery.js` will be executed
and you will have a chance to use the [HTTP API](webservice.md) to fix your error without flashing. Recovery will enable wifi if it was not already enabled.

Only modify `recovery.js` if you really think you know what you are doing (otherwise you will be flashing).
