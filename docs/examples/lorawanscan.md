# LoRaWanScan

This example requires some experience with LoRaWAN and
The Things Network (TTN).

This example shows how to use:
- LoRa API
- LoRaWan Library
- Timer Library

By default this demo will receive and decode ClassB Beacons.
No TTN account is required for this.

To run the full LoRaWANScan example edit lorawanscan.js
and set the mode to 0 and add the device details from your TTN account.

Requirements:
- [TTN](https://www.thethingsnetwork.org/) account

The idea behind this example is the following.
A LoRaWAN gateway only listens on a few channels (frequencies).
The LoRaWANScan will cycle thru every channel and send a small
message with the ACK flag set. After sending the message
it will listen on the corresponding down link channel and
record if an ACK was received. At the end it will print all channels ACKs where received on.

The example is not very useful but provides you with a demo
of two way communication LoRaWAN.

## Running it

Set the DeviceAddr, NetworkKey, and AppKey in [lorawanscan.js](../../spiffs_image/lorawanscan.js) and upload lorawanscan.js to your Fluxn0de.
Make sure to disable the frame counter.

Commands (replace 192.168.4.1 with the IP of your Fluxn0de):

```
curl http://192.168.4.1/file?lorawanscan.js --data-binary @spiffs_image/lorawanscan.js
curl http://192.168.4.1/control?setload=/lorawanscan.js
curl http://192.168.4.1/control?reset
```