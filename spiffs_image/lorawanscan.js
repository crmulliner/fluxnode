/*
 * Copyright: Collin Mulliner
 */

var wifi_ssid = "";
var wifi_pass = "";

var DeviceAddrHex = "260CA79F";
var AppKeyHex = "DE894188CC2A67CFBDABA74BF2F3D839";
var NwkKeyHex = "DEBBB7A1904014AAB8E068140BE2931E";


var mode = 2; // 0 = scan, 1 = listen for class B beacon, 2 = listen on RX2 (class C)

// --- do not change below ---
var scanState = {};
var packetCallback = processLora;

function OnStart() {

    Platform.loadLibrary("/wifimgnt.js");
    Platform.loadLibrary("/lorawanlib.js");
    Platform.loadLibrary("/timer.js");
    Platform.loadLibrary("/util.js");

    startWifi(wifi_ssid, wifi_pass);

    if (mode == 0) {
        if (AppKeyHex == "" || NwkKeyHex == "" || DeviceAddrHex == "") {
            print("\n\nConfiguration Needed!\n\nSet DeviceAddr, AppKey, and NwkKey from your LoraWAN console!!\n\n");
            print("Only ABP is supported!\n\n")
            return;
        }

        startScan();
        packetCallback = processLora;
    }

    if (mode == 1) {
        print("Listening for Beacons...\n");
        scanState = { channel: -1 };
        if (new Date().getTime() > __gpsEpoch) {
            var bChan = loraWanBeaconGetNextChannel();
            print(JSON.stringify(bChan) + "\n");
            scanState.channel = bChan.channel - 1;
        }
        listenNext();
        packetCallback = processListen;
    }

    if (mode == 2) {
        print("Listen Rx2\n");
        var addr = hexToBin(DeviceAddrHex);
        var appkey = hexToBin(AppKeyHex);
        var nwkkey = hexToBin(NwkKeyHex);

        scanState = {
            lp: LoraWanPacket(addr, nwkkey, appkey),
            channel: -1,
            channel_max: 71,
            answers: [],
        };
        packetCallback = processDownlink;
        listenRX2();
    }
}

function startScan() {
    print("Start scanning...\n");
    var addr = hexToBin(DeviceAddrHex);
    var appkey = hexToBin(AppKeyHex);
    var nwkkey = hexToBin(NwkKeyHex);

    scanState = {
        lp: LoraWanPacket(addr, nwkkey, appkey),
        channel: -1,
        channel_max: 71,
        answers: [],
    };

    setTimeout(scanNext, 5000);
}

function scanNext() {
    print("scanNext...\n");
    if (scanState.channel < scanState.channel_max) {
        scanState.channel++;
        var enc = new TextEncoder();
        var pkt = scanState.lp.confirmedUp(enc.encode("ping"), 1, scanState.channel, []);
        sendUp(pkt, scanState.channel);
        if (scanState.channel <= scanState.channel_max) {
            setTimeout(scanNext, 5000);
        } else {
            setTimeout(scanNext, 7000);
        }
    } else {
        print("scan done, answers on " + scanState.answers.length + " channels:");
        print(JSON.stringify(scanState.answers) + "\n");
    }
}

function processLora(pkt) {
    var res = scanState.lp.parseDownPacket(pkt);
    if (res.error == false && res.ack == true && res.micVerified == true) {
        print("got response for channel: " + scanState.channel + "\n");
        scanState.answers.push(scanState.channel);
        print(JSON.stringify(res) + "\n");
    }
}

function sendUp(pkt, channel) {
    var chans = loraWanUpDownChannel915(channel);
    print("sending on: " + chans.upFreq + "\n");
    LoRa.loraIdle();
    if (!LoRa.setFrequency(chans.upFreq)) {
        print("send freq error\n");
    }

    LoRa.setBW(chans.bw);
    LoRa.setSF(chans.sf);
    LoRa.setTxPower(17);
    LoRa.setPreambleLen(8);
    LoRa.setIQMode(false);
    LoRa.setSyncWord(0x34);
    LoRa.setCRC(true);
    LoRa.loraReceive();
    LoRa.sendPacket(Uint8Array.plainOf(pkt));

    LoRa.loraIdle();
    LoRa.setBW(500E3);
    LoRa.setIQMode(true);
    print("listening on: " + chans.downFreq + "\n");
    if (!LoRa.setFrequency(chans.downFreq)) {
        print("recv freq error\n");
    }
    LoRa.loraReceive();
}

function processListen(pkt) {
    print("Channel: " + scanState.channel + "\n");
    print("Data: " + binToHex(pkt) + "\n");
    var beacon = loraWanBeaconDecode(pkt, 0);
    if (beacon.time_crc) {
        Platform.setSystemTime(beacon.time + 1);
    }
    print(JSON.stringify(beacon) + "\n");
    print(new Date() + "\n");
    listenNext();
}

function listenRX2() {
    var chans = loraWanUpDownChannel915(0);
    LoRa.loraIdle();

    LoRa.setSF(8);
    LoRa.setTxPower(1);
    LoRa.setPreambleLen(8);
    LoRa.setSyncWord(0x34);
    LoRa.setCRC(true);
    LoRa.setBW(500E3);
    LoRa.setIQMode(true);
    print("Rx2 listening on: " + chans.down2Freq + "\n");
    if (!LoRa.setFrequency(chans.down2Freq)) {
        print("recv freq error\n");
    }
    LoRa.loraReceive();
}

function processDownlink(pkt) {
    print("Data: " + binToHex(pkt) + "\n");
    var dp = scanState.lp.parseDownPacket(pkt);
    print(JSON.stringify(dp) + "\n");
}

function listenNext() {
    if (scanState.channel < 7) {
        scanState.channel++;
    } else {
        scanState.channel = 0;
    }
    loraWanBeaconListen(scanState.channel, 0);
}

function OnEvent(event) {
    print(JSON.stringify(event) + "\n");
    if (event.EventType == 0) {
        packetCallback(event.EventData);
    }
}
