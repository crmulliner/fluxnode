# LoraWanPacket

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


## Methods

- [LoraWanPacket](#lorawanpacketdevaddrnwkkeyappkey)
- [LoraWanPacket.ackPacket](#lorawanpacket.ackpacketfportfcnt)
- [LoraWanPacket.confirmedUp](#lorawanpacket.confirmeduppayloadfportfcnt)
- [LoraWanPacket.makePacketFromPayLoad](#lorawanpacket.makepacketfrompayloadpayloadpkttypefportfcntlfcntdir)
- [LoraWanPacket.parseDownPacket](#lorawanpacket.parsedownpacketpacket)
- [LoraWanPacket.unConfirmedUp](#lorawanpacket.unconfirmeduppayloadfportfcnt)
- [arrayEqual](#arrayequalab)
- [binToHex](#bintohexbin)
- [hexToBin](#hextobinhex)
- [htons](#htonsnumber)
- [loraWanChannel915](#lorawanchannel915channeldirection)
- [loraWanUpDownChannel915](#lorawanupdownchannel915channel)
- [reverse](#reversebin)

---

## LoraWanPacket(devAddr,nwkKey,appKey)

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


- devAddr

  type: plain buffer

  device address

- nwkKey

  type: plain buffer

  network key

- appKey

  type: plain buffer

  application key

**Returns:** LoraWanPacket object

```
addr = hexToBin('AABBCCDD');
appkey = hexToBin('0011223344556677889900aabbccddee');
nwkkey = hexToBin('ffeeddccbbaa00998877665544332211');
var lwp = LoraWanPacket(addr, nwkkey, appkey);

```

## LoraWanPacket.ackPacket(fport,fcnt)

Create a UpACK packet.

- fport

  type: uint

  fport

- fcnt

  type: uint

  frame counter

**Returns:** loraWan packet as a plain buffer

```
pkt = lwp.ackPacket(1, 1);

```

## LoraWanPacket.confirmedUp(payload,fport,fcnt)

Create a ConfirmedUp packet.

- payload

  type: plain buffer

  payload

- fport

  type: uint

  fport

- fcnt

  type: uint

  frame counter

**Returns:** loraWan packet as a plain buffer

```
// make confirmed Up
var enc = new TextEncoder();
var payload = enc.encode('hi');
var vport = 1;
var fcnt = 1;
pkt = lwp.confirmedUp(payload, fport, fcnt);

```

## LoraWanPacket.makePacketFromPayLoad(payload,pkttype,fport,fcntl,fcnt,dir)

Create a LoraWan packet.

- payload

  type: plain buffer

  payload

- pkttype

  type: uint8

  packet type

- fport

  type: uint

  fport

- fcntl

  type: plain buffer

  fcntl

- fcnt

  type: uint16

  frame counter

- dir

  type: int

  direction 0 = up, 1 = down

**Returns:** loraWan packet as a plain buffer

```
// make un-confirmed Up
var enc = new TextEncoder();
var payload = enc.encode('hi');
var vport = 1;
var fcnt = 1;
pkt = lwp.makePacketFromPayLoad(payload, 0x40, fport, 0, fcnt, 0);

```

## LoraWanPacket.parseDownPacket(packet)

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


- packet

  type: plain buffer

  packet

**Returns:** decoded packet object

```
decodedPkt = lwp.parseDownPacket(pkt)
if (!decodedPkt.micVerified) {
    print('packet integrity check failed\n');
}

```

## LoraWanPacket.unConfirmedUp(payload,fport,fcnt)

Create a UnConfirmedUp packet.

- payload

  type: plain buffer

  payload

- fport

  type: uint

  fport

- fcnt

  type: uint

  frame counter

**Returns:** loraWan packet as a plain buffer

```
// make un-confirmed Up
var enc = new TextEncoder();
var payload = enc.encode('hi');
var vport = 1;
var fcnt = 1;
pkt = lwp.unConfirmedUp(payload, fport, fcnt);

```

## arrayEqual(a,b)

compare two arrays

- a

  type: array

  a

- b

  type: array

  b

**Returns:** boolean

```

```

## binToHex(bin)

convert a plain buffer to a hex string

- bin

  type: plainbuffer

  binary buffer

**Returns:** string

```

```

## hexToBin(hex)

convert hex string to a plain buffer

- hex

  type: string

  hex string

**Returns:** plain buffer

```

```

## htons(number)

convert host to network byte order short (uint16)

- number

  type: uint16

  uint16

**Returns:** uint16

```

```

## loraWanChannel915(channel,direction)

Get the US-915 frequency for a given channel and direction

- channel

  type: int

  0 - 71

- direction

  type: int

  0 = up 1 = down

**Returns:** frequency

```
var freq = loraWanChannel915(1, 0);

```

## loraWanUpDownChannel915(channel)

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


- channel

  type: int

  0 - 71

**Returns:** channel object for given channel

```
var loraRadioSettings = loraWanUpDownChannel915(1);

```

## reverse(bin)

reverse the bytes in a plain buffer

- bin

  type: plainbuffer

  binary buffer

**Returns:** plain buffer

```

```

