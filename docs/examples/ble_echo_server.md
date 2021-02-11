# BLE Echo Server

The BLE echo server does exactly what you think it does.

The echo server will accept the first BLE bonding request.
Everything sent to the [BLE 'websocket' service](../BLE.md)
will be sent back. Checkout: [ble_echo_server.js](../../spiffs_image/ble_echo_server.js)

## Running it

Commands (replace 192.168.4.1 with the IP of your Fluxn0de):

```
curl http://192.168.4.1/control?setload=/ble_echo_server.js
curl http://192.168.4.1/control?reset
```

Now you should see Fluxn0de BLE advertisement packets.

Connect to the BLE service and start sending data.

## BLE Battery Service

If you have a BLE device that can query standard BLE services
such as the battery service you should be able to get a reading from Fluxn0de.
