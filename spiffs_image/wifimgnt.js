/*
 * Simple wifi management for the examples.
 */

function startWifi(ssid, pass) {
    if (Platform.getConnectivity() != 0) {
        print("Wifi already configured\n");
        return;
    }

    if (ssid == "" || pass == "") {
        wifiAP();
        return;
    }

    print("Connecting to Wifi Network: '" + ssid + "'\n");
    Platform.setConnectivity(1);
    if (!Platform.wifiConfigure(ssid, pass, 1, 0)) {
        print("falling back to AP mode...\n");
        wifiAP();
        return;
    }
    print("FluxN0de IP: " + Platform.getLocalIP() + "\n");
}

function wifiAP() {
    var wifi_ssid = "fluxn0de";
    var wifi_pass = "fluxn0de";
    print("\n\nFluxN0de starting WIFI AP SSID: " + wifi_ssid + " Password: " + wifi_pass + "\n\n");
    Platform.setConnectivity(1);
    if (Platform.wifiConfigure(wifi_ssid, wifi_pass, 0, 0)) {
        print("FluxN0de IP: " + Platform.getLocalIP() + "\n");
        Platform.setLEDBlink(0, 1, 0.5);
    } else {
        print("\n\nWifi AP error, password length < 8?\n\n");
    }
}
