# FileSystem Test

filesystem_test.js is a simple application that shows you how
to use the [FileSystem](../filesystem.md) API. Checkout: [filesystem_test.js](../../spiffs_image/filesystem_test.js).

# Running it

Commands (replace 192.168.4.1 with the IP of your Fluxn0de):

```
curl http://192.168.4.1/control?setload=/filesystem_test.js
curl http://192.168.4.1/control?reset
```
