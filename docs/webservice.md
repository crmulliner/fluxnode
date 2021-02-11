# Web Service

The Web Service provides a minimal HTTP server to implement a web application.
The web service includes a websocket server that allows for one connection. 
The websocket connection is transparently provided to the JavaScript 
runtime via the OnEvent() function.

The websocket server is listening on port `8888`.

`192.168.4.1` is the default IP for Fluxn0de in Wifi AP mode.

The `web_mode` parameter of Platform.wifiConfigure() allows to disable `/control` and 
POST to `/file`.

## API

The API will not allow to read and write files that start with `_`.
This allows the device to store `private` data.

### URL: /

- GET will read /index.html

### /file?\<filename\>

- GET to read \<filename\>
- POST to write to \<filename\>

### /control?\<command\>

- GET to execute the \<command\>

Commands:
- **reset** (reset the JavaScript runtime, same as Platform.reset())
- **setload=\<filename\>** (set the load file, same as Platform.setLoadFileName(filename))
- **reboot** (reboot the board, same as Platform.reboot())
- **deletefile=\<filename\>** (delete \<filename\>, same as FileSystem.unlink(filename))

Example:
```

Upload test42.js to the board.
$ http://192.168.4.1/file?test42.js --data-binary @test42.js
OK

Set /test42.js as the application to run
$ http://192.168.4.1/control?setload=/test42.js
OK

Reset JavaScript runtime and run test42.js
$ curl http://192.168.4.1/control?reset
OK

```


