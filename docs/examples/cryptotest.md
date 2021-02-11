# Cryptotest

This is a simple application to test the [Crypto API](../crypto.md). This will test AES-128 ECB, CMAC via AES-128, and the
random number generator. Checkout: [cryptotest.js](../../spiffs_image/cryptotest.js).

# Running it

Commands (replace 192.168.4.1 with the IP of your Fluxn0de):

```
curl http://192.168.4.1/control?setload=/cryptotest.js
curl http://192.168.4.1/control?reset
```
