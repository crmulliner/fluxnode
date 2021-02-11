/*
 * Copyright: Collin Mulliner
 */

/* change to load a different application */
var app = "/test.js";

function OnStart() {
    print("BoardName: " + Platform.BoardName + "\n");
    print("HeapFree: " + Platform.getFreeHeap()/1000 + " KB\n");
    print("loading: " + app + "\n");
    Platform.setLoadFileName(app);
    Platform.reset();
    print("reseting runtime\n");
}

function OnTimer() {
}

function OnEvent(event) {
    print(JSON.stringify(event));
}
