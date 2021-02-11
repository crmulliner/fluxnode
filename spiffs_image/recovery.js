/*
 * Copyright: Collin Mulliner
 */

/* set clientSSID and clientPassword to connect to a Wifi AP in recovery */
var clientSSID = "";
var clientPassword = "";

function APRecovery() {
    var wifi_ssid = "fluxn0de";
    var wifi_pass = "fluxn0de";
    print("\n\nFlux starting recovery WIFI AP SSID: '" + wifi_ssid + "' Password: '" + wifi_pass + "'\n\n");
    Platform.setConnectivity(1);
    if (Platform.wifiConfigure(wifi_ssid, wifi_pass, 0, 0)) {
        print("FluxN0de IP: " + Platform.getLocalIP() + "\n");
    } else {
        print("\n\nWifi AP error, password length < 8?\n\n");
    }
}

function ClientRecovery() {
    if (clientSSID != "") {
        print("Client Recovery\n");
        Platform.setConnectivity(1);
        Platform.wifiConfigure(clientSSID, clientPassword, 1, 0);
        return true;
    }
    print("\n\nSet clientSSID and clientPassword to use ClientRecovery\n\n");
    return false;
}

function OnStart() {
    if (Platform.getConnectivity() != 1) {
        if (!ClientRecovery()) {
            APRecovery();
        }
    }
    print("--- Recovery ---\n");
    Platform.setLEDBlink(0, 1, 0.5);
}

function OnEvent(event) {
    print(JSON.stringify(event));
    /* reboot when button is pressed */
    if (event.EventType == 4) {
        Platform.reboot();
    }
}
