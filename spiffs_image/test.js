/*
 * Copyright: Collin Mulliner
 */


var wifi_ssid = "";
var wifi_pass = "";

var logs = [];

function OnStart() {
    print("NOW running: test.js\n");

    Platform.loadLibrary("/wifimgnt.js");
    startWifi(wifi_ssid, wifi_pass);

    if (Platform.ButtonNum == 0) {
        Platform.loadLibrary("/timer.js");
        intervalTimer(sendInfo, 7000);
    } else {
        print("Press Button!\n");
    }
}

function sendInfo() {
    var d = new Date();
    var x = {
        cmd: "info",
        battery_percent: Platform.getBatteryPercent(),
        battery_mv: Math.floor(Platform.getBatteryMVolt()),
        usb_connected: Platform.getUSBStatus(),
        battery_charging: Platform.getBatteryStatus(),
        heap_free: Platform.getFreeHeap(),
        heap_internal_free: Platform.getFreeInternalHeap(),
        fs_total: FileSystem.fsSizeTotal(),
        fs_used: FileSystem.fsSizeUsed(),
        local_ip: Platform.getLocalIP(),
        connectivity: Platform.getConnectivity(),
        client_id: Platform.getClientID(),
        boardname: Platform.BoardName,
        system_time: d.getTime(),
        wifi_mac: Platform.wifiGetMacAddr(),
        ble_addr: Platform.bleGetDeviceAddr(),
        flash_size: Platform.FlashSize,
    };
    print(JSON.stringify(x) + "\n");
    Platform.sendEvent(1, Uint8Array.allocPlain(JSON.stringify(x)));
}

function OnEvent(evt) {
    if (EventName(evt) == "button") {
        sendInfo();
        return;
    }
    if (EventName(evt) == "ui") {
        var dec = new TextDecoder();
        var cmd = JSON.parse(dec.decode(evt.EventData));
        switch (cmd.cmd) {
            case "set_system_time":
                Platform.setSystemTime(cmd.time);
                break;
            default:
                print("command '" + cmd.cmd + "' not supported\n");
                break;
        }
        return;
    }
}

function log(line) {
    logs.push(line);
    sendLogs();
}

function sendLogs() {
    if (Platform.getClientConnected()) {
        for (var i = 0; i < logs.length; i++) {
            var x = JSON.stringify({ cmd: "log", event: logs[i] });
            Platform.sendEvent(1, Uint8Array.allocPlain(x));
        }
        logs = [];
    }
}

function EventName(event) {
    var et = ['lora', 'ui', 'ui_connected', 'ui_disconnected', 'button'];
    return et[event.EventType];
}
