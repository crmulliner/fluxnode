/*
 * Copyright: Collin Mulliner
 */

/* jsondoc
{
"class": "LoraWanPacket",
"longtext":"
This is a very basic LoRaWAN library.

Supported features:
- Activation Method: ABP
- send confirmed / un-confirmed message
- receive confirmed / un-confirmed message
- encode/decode ACK message

Unsupported:
- Activation Method: OTAA
- Sending FOpts
- Decoding received FOpts

"
}
*/

/* jsondoc
{
"name": "LoraWanPacket",
"args": [
{"name": "devAddr", "vtype": "plain buffer", "text": "device address"},
{"name": "nwkKey", "vtype": "plain buffer", "text": "network key"},
{"name": "appKey", "vtype": "plain buffer", "text": "application key"}
],
"longtext": "
Create a LoraWanPacket instance using device address, network key, and application key.
All packet operations will use the address and keys to encode and decode packets.

The instance will contain the following members:
```
 {
    devAddr: plainBuffer,
    nwkKey: plainBuffer,
    appKey: plainBuffer,
    makePacketFromPayLoad: function,
    unConfirmedUp: function,
    confirmedUp: function,
    ackPacket: function,
    parseDownPacket: function,
}
```
",
"return": "LoraWanPacket object",
"example": "
addr = hexToBin('AABBCCDD');
appkey = hexToBin('0011223344556677889900aabbccddee');
nwkkey = hexToBin('ffeeddccbbaa00998877665544332211');
var lwp = LoraWanPacket(addr, nwkkey, appkey);
"
}
*/
function LoraWanPacket(devAddr, nwkKey, appKey) {
    return {
        devAddr: reverse(devAddr),
        nwkKey: nwkKey,
        appKey: appKey,
        makePacketFromPayLoad: __makePacketFromPayLoad,
        unConfirmedUp: __unConfirmedUp,
        confirmedUp: __confirmedUp,
        ackPacket: __makeAck,
        parseDownPacket: __parseDownPacket,
    }
}

/* jsondoc
{
"name": "LoraWanPacket.parseDownPacket",
"args": [
{"name": "packet", "vtype": "plain buffer", "text": "packet"}   ],
"return": "decoded packet object",
"longtext":"
Decode a received LoraWan `down` packet.

The decoded object packet contains the following elements:
```
{
    error: boolean,
    devAddr: plainBuffer,
    confirmed: boolean,
    payload: plainBuffer,
    fport: int,
    fcnt: int,
    fops: plainBuffer,
    ack: boolean,
    fpending: boolean,
    adr: boolean,
    adrackreq: boolean,
    micVerified: boolean,
}
```
",
"example": "
decodedPkt = lwp.parseDownPacket(pkt)
if (!decodedPkt.micVerified) {
    print('packet integrity check failed\\n');
}
"
}
*/
function __parseDownPacket(pkt) {
    if (pkt.length < 12) {
        return {
            error: true,
            errMsg: "packet too short",
        };
    }
    var MHDR = pkt[0];
    if ((MHDR >> 5) != 5 && (MHDR >> 5) != 3) {
        return {
            error: true,
            errMsg: "MType bad",
        };
    }
    var addr = new Uint8Array(4);
    addr.set(pkt.subarray(1, 5), 0);
    var FCTRL = pkt[5];
    var buf = new ArrayBuffer(2);
    var fcntbytes = new DataView(buf);
    fcntbytes.setUint8(0, pkt[6]);
    fcntbytes.setUint8(1, pkt[7]);
    var FCNT = htons(fcntbytes.getUint16(0));
    var FOPSLEN = FCTRL & 0x0f;
    //print(FOPSLEN);
    var FOPS = new Uint8Array(FOPSLEN);
    var data_idx = 8;

    if (FOPSLEN > 0 && FOPSLEN + 12 <= pkt.length) {
        FOPS.set(pkt.subarray(data_idx, data_idx + FOPSLEN), 0);
        data_idx += FOPSLEN;
    }

    var micOk = __checkMic(pkt, this.nwkKey, FCNT, addr, 1);
    var payload = new Uint8Array(0);
    var FPORT = new Uint8Array(0);
    if (micOk && (data_idx + 4 + 1 < pkt.length)) {
        FPORT = pkt[data_idx];
        data_idx++;
        var pay = new Uint8Array(pkt.length - 4 - data_idx);
        pay.set(pkt.subarray(data_idx, pkt.length - 4), 0);
        payload = __encryptPayload(pay, addr, this.appKey, FCNT, 1);
    }

    return {
        error: false,
        devAddr: addr,
        confirmed: (MHDR >> 5) == 5,
        payload: payload,
        fport: FPORT,
        fcnt: FCNT,
        fops: FOPS,
        ack: (FCTRL & 0x20) != 0,
        fpending: (FCTRL & 0x10) != 0,
        adr: (FCTRL & 0x80) != 0,
        adrackreq: (FCTRL & 0x40) != 0,
        micVerified: micOk,
    };
}

/* jsondoc
{
"name": "LoraWanPacket.unConfirmedUp",
"args": [
{"name": "payload", "vtype": "plain buffer", "text": "payload"},
{"name": "fport", "vtype": "uint", "text": "fport"},
{"name": "fcnt", "vtype": "uint", "text": "frame counter"}
    ],
"return": "loraWan packet as a plain buffer",
"text": "Create a UnConfirmedUp packet.",
"example": "
// make un-confirmed Up
var enc = new TextEncoder();
var payload = enc.encode('hi');
var vport = 1;
var fcnt = 1;
pkt = lwp.unConfirmedUp(payload, fport, fcnt);
"
}
*/
function __unConfirmedUp(payload, fport, fcnt) {
    return this.makePacketFromPayLoad(payload, 0x40, fport, 0, fcnt, 0);
}

/* jsondoc
{
"name": "LoraWanPacket.confirmedUp",
"args": [
{"name": "payload", "vtype": "plain buffer", "text": "payload"},
{"name": "fport", "vtype": "uint", "text": "fport"},
{"name": "fcnt", "vtype": "uint", "text": "frame counter"}
    ],
"return": "loraWan packet as a plain buffer",
"text": "Create a ConfirmedUp packet.",
"example": "
// make confirmed Up
var enc = new TextEncoder();
var payload = enc.encode('hi');
var vport = 1;
var fcnt = 1;
pkt = lwp.confirmedUp(payload, fport, fcnt);
"
}
*/
function __confirmedUp(payload, fport, fcnt) {
    return this.makePacketFromPayLoad(payload, 0x80, fport, 0, fcnt, 0);
}

/* jsondoc
{
"name": "LoraWanPacket.ackPacket",
"args": [
{"name": "fport", "vtype": "uint", "text": "fport"},
{"name": "fcnt", "vtype": "uint", "text": "frame counter"}
    ],
"return": "loraWan packet as a plain buffer",
"text": "Create a UpACK packet.",
"example": "
pkt = lwp.ackPacket(1, 1);
"
}
*/
function __makeAck(fport, fcnt) {
    return this.makePacketFromPayLoad(new Uint8Array(0), 0x40, fport, 0x20, fcnt, 0);
}

/* jsondoc
{
"name": "LoraWanPacket.makePacketFromPayLoad",
"args": [
{"name": "payload", "vtype": "plain buffer", "text": "payload"},
{"name": "pkttype", "vtype": "uint8", "text": "packet type"},
{"name": "fport", "vtype": "uint", "text": "fport"},
{"name": "fcntl", "vtype": "plain buffer", "text": "fcntl"},
{"name": "fcnt", "vtype": "uint16", "text": "frame counter"},
{"name": "dir", "vtype": "int", "text": "direction 0 = up, 1 = down"}
    ],
"return": "loraWan packet as a plain buffer",
"text": "Create a LoraWan packet.",
"example": "
// make un-confirmed Up
var enc = new TextEncoder();
var payload = enc.encode('hi');
var vport = 1;
var fcnt = 1;
pkt = lwp.makePacketFromPayLoad(payload, 0x40, fport, 0, fcnt, 0);
"
}
*/
// no support for Fopts
function __makePacketFromPayLoad(payload, pkttype, fport, fctrl, fcnt, dir) {
    var MHDR = new Uint8Array(1);
    MHDR[0] = pkttype;
    var FHDR = __makeFhdrNoFopts(this.devAddr, fctrl, fcnt);
    var FPORT = __makeFport(fport);

    var pkt = new Uint8Array(MHDR.length + FHDR.length + FPORT.length + payload.length);
    pkt.set(MHDR, 0);
    pkt.set(FHDR, 1);
    pkt.set(FPORT, MHDR.length + FHDR.length);
    if (payload.length > 0) {
        var encPayload = __encryptPayload(payload, this.devAddr, this.appKey, fcnt, dir);
        pkt.set(encPayload, pkt.length - encPayload.length);
    }
    var mic = __makeMic(pkt, this.devAddr, this.nwkKey, fcnt, dir);
    var lorapkt = new Uint8Array(pkt.length + mic.length);
    lorapkt.set(pkt, 0);
    lorapkt.set(mic, pkt.length);
    return lorapkt;
}

function __checkMic(pkt, key, fcnt, addr, dir) {
    var pkt_wo_mic = new Uint8Array(pkt.length - 4);
    pkt_wo_mic.set(pkt.subarray(0, pkt.length - 4), 0);
    var old_mic = new Uint8Array(4);
    old_mic.set(pkt.subarray(pkt.length - 4, pkt.length));
    var mic = __makeMic(pkt_wo_mic, addr, key, fcnt, dir);
    return arrayEqual(mic, old_mic);
}

function __makeMic(pkt, addr, key, fcnt, dir) {
    var msg = new Uint8Array(pkt.length + 16);
    var tmp = __makeEncBase(dir, fcnt, addr, 0);
    tmp[0] = 0x49;
    tmp[15] = pkt.length;
    msg.set(tmp, 0);
    msg.set(pkt, 16);
    var cmac = new Uint8Array(16);
    Crypto.aes128Cmac(key, msg, cmac);
    var mic = new Uint8Array(4);
    mic.set(cmac.subarray(0, 4), 0);
    return mic;
}

function __makeFport(port) {
    var fport = new Uint8Array(1);
    fport[0] = port;
    return fport;
}

// Fopts not supported
function __makeFhdrNoFopts(addr, fctrl, fcnt) {
    // ADDR (4) FCTRL (1) FCnt (2) Fopts (0-15)
    var FHDR = new Uint8Array(7);
    FHDR.set(addr, 0);
    FHDR[4] = fctrl;
    var tmp = new ArrayBuffer(2);
    new DataView(tmp).setUint16(0, htons(fcnt));
    FHDR.set(tmp, 5);
    return FHDR;
}

function __encryptPayload(payload, addr, key, fcnt, dir) {
    var blocks = Math.ceil(payload.length / 16);
    var keyStream = new Uint8Array(16 * blocks);

    for (var block = 0; block < blocks; block++) {
        var part = __makeEncBase(dir, fcnt, addr, block);
        Crypto.aes128EcbEnc(key, part);
        keyStream.set(part, block * 16);
    }

    var encPayload = new Uint8Array(payload.length);
    for (var i = 0; i < payload.length; i++) {
        encPayload[i] = (keyStream[i] ^ payload[i]);
    }
    return encPayload;
}

function __makeEncBase(dir, fcnt, addr, blocknum) {
    var base = new Uint8Array(16);
    base[0] = 0x01;
    base[1] = 0x00;
    base[2] = 0x00;
    base[3] = 0x00;
    base[4] = 0x00;

    // dir
    base[5] = 0x00;
    if (dir == 1) {
        base[5] = 0x01;
    }

    base.set(addr, 6);

    // fcnt
    var tmp = new ArrayBuffer(2);
    new DataView(tmp).setUint16(0, htons(fcnt));
    base.set(tmp, 10);
    base[12] = 0x00;
    base[13] = 0x00;

    base[14] = 0x00;
    base[15] = blocknum + 1;
    return base;
}

/* jsondoc
{
"name": "loraWanChannel915",
"args": [
{"name": "channel", "vtype": "int", "text": "0 - 71"},
{"name": "direction", "vtype": "int", "text": "0 = up 1 = down"}
    ],
"return": "frequency",
"text": "Get the US-915 frequency for a given channel and direction",
"example": "
var freq = loraWanChannel915(1, 0);
"
}
*/
function loraWanChannel915(channel, dir) {
    if (dir == 0) {
        if (channel >= 0 && channel <= 63) {
            var freq = 9023 + (2 * channel);
            return freq / 10;
        }
        if (channel >= 64 && channel <= 71) {
            var freq = 9030 + (16 * (channel - 64));
            return freq / 10;
        }
    }
    if (channel >= 0 && channel <= 7) {
        var freq = 9233 + (6 * channel);
        return freq / 10;
    }
    return 0.0;
}

/* jsondoc
{
"name": "loraWanUpDownChannel915",
"args": [
{"name": "channel", "vtype": "int", "text": "0 - 71"}
    ],
"return": "channel object for given channel",
"longtext": "
Get the up,down, and 2nd down frequency for a given channel in US-915.
The channel object contains the following members:
```
{
    channel: uint,
    sf: uint,
    upFreq: double,
    downFreq: double,
    down2Freq: double,
}
```
",
"example": "
var loraRadioSettings = loraWanUpDownChannel915(1);
"
}
*/
function loraWanUpDownChannel915(channel) {
    var up = loraWanChannel915(channel, 0);
    var down = loraWanChannel915(channel % 8, 1);
    var sf = 7;
    if (channel > 63) {
        sf = 8;
    }
    return {
        channel: channel,
        sf: sf,
        upFreq: up,
        downFreq: down,
        down2Freq: 923.3,
    };
}

/* jsondoc
{
"name": "arrayEqual",
"args": [
{"name": "a", "vtype": "array", "text": "a"},
{"name": "b", "vtype": "array", "text": "b"}
    ],
"return": "boolean",
"text": "compare two arrays",
"example": "
"
}
*/
function arrayEqual(a, b) {
    if (a.length != b.length) {
        return false;
    }
    for (var i = 0; i < a.length; i++) {
        if (a[i] != b[i]) {
            return false;
        }
    }
    return true;
}

/* jsondoc
{
"name": "hexToBin",
"args": [
{"name": "hex", "vtype": "string", "text": "hex string"}
    ],
"return": "plain buffer",
"text": "convert hex string to a plain buffer",
"example": "
"
}
*/
function hexToBin(hexStr) {
    return Duktape.dec('hex', hexStr);
}

/* jsondoc
{
"name": "binToHex",
"args": [
{"name": "bin", "vtype": "plainbuffer", "text": "binary buffer"}
    ],
"return": "string",
"text": "convert a plain buffer to a hex string",
"example": "
"
}
*/
function binToHex(bin) {
    return Duktape.enc('hex', bin);
}

/* jsondoc
{
"name": "reverse",
"args": [
{"name": "bin", "vtype": "plainbuffer", "text": "binary buffer"}
    ],
"return": "plain buffer",
"text": "reverse the bytes in a plain buffer",
"example": "
"
}
*/
function reverse(input) {
    var output = new Uint8Array(input.length);
    for (var i = 0; i < input.length; i++) {
        output[i] = input[input.length - (i+1)];
    }
    return output;
}

/* jsondoc
{
"name": "htons",
"args": [
{"name": "number", "vtype": "uint16", "text": "uint16"}
    ],
"return": "uint16",
"text": "convert host to network byte order short (uint16)",
"example": "
"
}
*/
function htons(num) {
    return (num >> 8) | ((num & 0xff) << 8);
}
