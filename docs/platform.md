# Platform

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


## Methods

- [bleBondAllow](#blebondallowbond)
- [bleBondGetDevices](#blebondgetdevices)
- [bleBondRemove](#blebondremovebaddr)
- [bleGetDeviceAddr](#blegetdeviceaddr)
- [bleSetPasscode](#blesetpasscodepass)
- [getBatteryMVolt](#getbatterymvolt)
- [getBatteryPercent](#getbatterypercent)
- [getBatteryStatus](#getbatterystatus)
- [getBootime](#getbootime)
- [getClientConnected](#getclientconnected)
- [getClientID](#getclientid)
- [getConnectivity](#getconnectivity)
- [getFreeHeap](#getfreeheap)
- [getFreeInternalHeap](#getfreeinternalheap)
- [getLocalIP](#getlocalip)
- [getUSBStatus](#getusbstatus)
- [gpioRead](#gpioreadgpionum)
- [gpioWrite](#gpiowritegpionumvalue)
- [isButtonPressed](#isbuttonpressed)
- [loadLibrary](#loadlibraryfilename)
- [print](#printvar)
- [reboot](#reboot)
- [reset](#reset)
- [sendEvent](#sendeventevent_typedata)
- [setConnectivity](#setconnectivitycon)
- [setLED](#setledled_idonoff)
- [setLEDBlink](#setledblinkled_idratebrightness)
- [setLoadFileName](#setloadfilenamefilename)
- [setSystemTime](#setsystemtimetime)
- [setTimer](#settimertimeout)
- [stopLEDBlink](#stopledblinkled_id)
- [wifiConfigure](#wificonfiguressidpasswordmodeweb_mode)
- [wifiGetMacAddr](#wifigetmacaddr)

---

## bleBondAllow(bond)

Set BLE bond mode.

- bond

  type: uint

  allow = 1, disallow = 0

**Returns:** boolean status

```
// allow the next BLE bonding attempt
Platform.bleBondAllow(1);

```

## bleBondGetDevices()

Get BLE addresses that are currently bonded. This returns a string array.

**Returns:** array of strings

```
var devs = Platform.bleBondGetDevices();
print(devs);

```

## bleBondRemove(baddr)

Remove bonding for a BLE MAC.

- baddr

  type: string

  BLE MAC to remove

**Returns:** boolean status

```
Platform.bleBondRemove('00:11:22:33:44:55');

```

## bleGetDeviceAddr()

Get BLE local device address

**Returns:** string BLE address

```
var dev = Platform.bleGetDeviceAddr();

```

## bleSetPasscode(pass)

Set BLE passcode.

- pass

  type: uint

  BLE pin

**Returns:** boolean status

```
Platform.bleSetPasscode(1234);

```

## getBatteryMVolt()

Get the battery charge level in mV.

**Returns:** number

```
Platform.getBatteryMVolt();

```

## getBatteryPercent()

Get the battery charge level in percent.

**Returns:** number

```
Platform.getBatteryPercent();

```

## getBatteryStatus()

Get the battery charging status. 0 = draining, 1 = charging

**Returns:** number

```
if (Platform.getBatteryStatus() == 1) {
    print('battery is charging\n');
}

```

## getBootime()

get boot time

**Returns:** time of system boot up

```

```

## getClientConnected()

Get the UI client status. This is either the websocket or the BLE connection. 0 = disconnected, 1 = connected

**Returns:** number

```
if (Platform.getClientConnected() == 1) {
    print('client is connected\n');
}

```

## getClientID()

Get the ID of the client. Either an IP address or a BLE MAC, 'none' no client connected.

**Returns:** string

```
print('client ID is: ' + Platform.getClientID() + '\n');

```

## getConnectivity()

Get the connectivity mode. 0 = wifi/ble off, 1 = wifi, 2 = BLE.

**Returns:** number

```
if (Platform.getConnectivity() == 0) {
    print('disconnected from the world\n');
}

```

## getFreeHeap()

Returns number of free bytes on the heap.

**Returns:** number

```
print('we got ' + Platform.getFreeHeap()/1024 + 'K free heap');

```

## getFreeInternalHeap()

Returns number of free bytes on the internal heap.

**Returns:** number

```
print('we got ' + Platform.getFreeInternalHeap()/1024 + 'K free internal heap');

```

## getLocalIP()

Get the local IP of the board. If connectivity is Wifi.

**Returns:** string

```
print('my IP is: ' + Platform.getLocalIP() + '\n');

```

## getUSBStatus()

Get the the USB connection status. 0 = disconnected, 1 = connected

**Returns:** number

```
Platform.getUSBStats();

```

## gpioRead(gpionum)

Read from GPIO. This also configures the given GPIO as an input GPIO. Use with caution!

- gpionum

  type: uint

  GPIO number

**Returns:** number

```
Platform.gpioRead(14);

```

## gpioWrite(gpionum,value)

Write to GPIO. This also configures the given GPIO as an output GPIO. Use with caution!

- gpionum

  type: uint

  GPIO number

- value

  type: uint

  0 or 1

```
Platform.gpioWrite(15, 1);

```

## isButtonPressed()

Returns True if the button is pressed while this function is called.

**Returns:** boolean status

```
if (Platform.isButtonPressed()) {
    print('somebody is pressing the button right now');
}

```

## loadLibrary(filename)

Load JavaScript code into current application.

- filename

  type: string

  filename to load

**Returns:** boolean status

```
Platform.loadLibrary('/timer.js');

```

## print(var)

Print to the console. Note: Print is not part of the Platform namespace.

- var

  type: var

  multiple arguments

```
print('Hi\n');

```

## reboot()

Reboot the board.

```
Platform.reboot();

```

## reset()

Reset the JavaScript runtime. This will try to load and run `main.js`. If `main.js` can't be run, try to run `recovery.js`. The idea behind `recovery.js` is that you have a fallback application that will allow you to recover from a error without power cycle and/or flashing the board.

```
Platform.reset();

```

## sendEvent(event_type,data)

Send event to the connected UI client. Only UI event (1) is currently supported.

- event_type

  type: uint

  1 = UI event (send to websocket or BLE)

- data

  type: plain buffer

  data to send (see: https://wiki.duktape.org/howtobuffers2x)

**Returns:** boolean status

```
var x = {
    test: 'test string',
    num: 1337,
};
Platform.sendEvent(1, Uint8Array.allocPlain(JSON.stringify(x)));

```

## setConnectivity(con)

Set connectivity. 0 = off, 1 = wifi, 2 = BLE.

- con

  type: uint

  connectivity mode

**Returns:** boolean status

```
// switch to BLE
Platform.setConnectivity(2);

```

## setLED(led_id,onoff)

Set an LED on/off.

- led_id

  type: uint

  LED id (0-1)

- onoff

  type: uint

  on (1) / off (0) 

```
Platform.setLED(0, 1);

```

## setLEDBlink(led_id,rate,brightness)

Set an LED to blink at a given rate and brightness.

- led_id

  type: uint

  LED id (0-1)

- rate

  type: uint

  0-4

- brightness

  type: double

  0.0-1.0

```
Platform.setLEDBlink(0, 2, 0.5);

```

## setLoadFileName(filename)

Set the file to be loaded the next time reset() is called.

- filename

  type: string

  filename to load

```
// stop current application and load test.js
Platform.setLoadFileName('/test.js');
Platform.reset();

```

## setSystemTime(time)

Set the system time.

- time

  type: uint

  time (unix epoch)

```
Platform.setSystemTime(1580515200);

```

## setTimer(timeout)

Set timeout in milliseconds after which OnTimer() is called. The shortest timer is `10` milliseconds. Setting a timer of 0 will disable the timer.

- timeout

  type: uint

  milliseconds, 0 = disable timer

**Returns:** boolean status

```
// timer will go off in 5 seconds
Platform.setTimer(5000);

function OnTimer() {
    print('timer was called\n');
}

```

## stopLEDBlink(led_id)

Stop an LED blinking.

- led_id

  type: uint

  LED id (0-1)

```
Platform.stopLEDBlink(0);

```

## wifiConfigure(ssid,password,mode,web_mode)

Configure Wifi. The web_mode can be used to lock down the web API.

- ssid

  type: string

  WIFI SSID

- password

  type: string

  WPA password (min 8 characters)

- mode

  type: uint

  0 = AP, 1 = client

- web_mode

  type: uint

  webserver API readonly = 1, read/write = 0

**Returns:** boolean status

```
Platform.setConnectivity(1);
// start WIFI AP and set web mode to R/W
var success = Platform.wifiConfigure('fluxnode','fluxulf', 0, 1);
if (success == false) {
    print('wifi start failed, check SSID and password\n');
}

```

## wifiGetMacAddr()

Get WIFI MAC address

**Returns:** string MAC address

```
var dev = Platform.wifiGetMacAddr();

```

