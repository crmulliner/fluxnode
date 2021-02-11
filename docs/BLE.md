# BLE

Fluxn0de implements two services: battery level service and a non standard service that
allows exchanging frames similar to a websocket connection.

## BLE 'Websocket'

The BLE 'websocket' provides a websocket like service to send and receive data frames.
Each frame starts with 16-bit length (network byte order) indicator followed by the number of bytes indicated
by the length.

### Service UUIDs

The general service UUID is: `0xF700`

Send data to the Fluxn0de via UUID: `0xF701`
Fluxn0de receives data via unconfirmed writes. It does **NOT** support
prepare/execute.

Receive data from Fluxn0de via UUID: `0xF702`
Fluxn0de sends data via unconfirmed notifications.


## BLE Battery Service

Provides the standardized battery service using UUID `0x180F`
