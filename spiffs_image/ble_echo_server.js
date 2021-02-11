/*
 * Copyright: Collin Mulliner
 */

function OnStart() {
    Platform.setConnectivity(2);
    // allow next bonding request
    Platform.bleBondAllow(1);
    print("BLE echo server...\n");
}

function OnEvent(evt) {
    if (evt.EventType == 1) {
        print("data received " + evt.EventData.length + " bytes\n");
        // send back data we just received (echo service)
        Platform.sendEvent(1, evt.EventData);
    }
}
