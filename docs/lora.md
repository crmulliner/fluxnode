# LoRa

Documentation for the LoRa modem API.

The library supports every feature needed for LoRaWAN.
Additionally it supports Frequency hopping spread spectrum (FHSS), see: [setHopping](#sethoppinghopshopfreqs).
The library has been tested on [RFM95](https://www.hoperf.com/modules/lora/RFM95.html).


## Methods

- [loraIdle](#loraidle)
- [loraReceive](#lorareceive)
- [loraSleep](#lorasleep)
- [sendPacket](#sendpacketpacket_bytes)
- [setBW](#setbwbw)
- [setCR](#setcrcr)
- [setCRC](#setcrccrc)
- [setFrequency](#setfrequencyfreq)
- [setHopping](#sethoppinghopshopfreqs)
- [setIQMode](#setiqmodeiq_invert)
- [setPayloadLen](#setpayloadlenlength)
- [setPreambleLen](#setpreamblelenlength)
- [setSF](#setsfsf)
- [setSyncWord](#setsyncwordsyncword)
- [setTxPower](#settxpowerlevel)

---

## loraIdle()

Set LoRa modem to idle.

```
LoRa.loraIdle();

```

## loraReceive()

Set LoRa modem to Receive.Packets will arrive via OnEvent().The type of EventData is plain buffer, see: https://wiki.duktape.org/howtobuffers2x

```
function OnStart() {
  LoRa.loraReceive();
}
function OnEvent(evt) {
  if (evt.EventType == 1) {
    pkt = evt.EventData;
    rssi = evt.LoRaRSSI;
    timestamp = evt.TimeStamp;
  }
}

```

## loraSleep()

Set LoRa modem to sleep. Lowest power consumption.

```
LoRa.loraSleep();

```

## sendPacket(packet_bytes)

Send a LoRa packet. LoRa.loraReceive() has to be called before sending. The modem can be put into idle or sleep right after sendPacket returns. For plain buffers see: https://wiki.duktape.org/howtobuffers2x

- packet_bytes

  type: Plain Buffer

  packet bytes length 1-255

```
LoRa.sendPacket(Uint8Array.plainOf('Hello'));

```

## setBW(bw)

Set the bandwidth.

- bw

  type: uint

  bandwidth 7.8E3 - 5000E3

**Returns:** boolean status

```
LoRa.setBW(125E3);

```

## setCR(cr)

Set the coding rate.

- cr

  type: uint

  coding rate 5 - 8

**Returns:** boolean status

```
LoRa.setCR(5);

```

## setCRC(crc)

Enable / Disable packet CRC.

- crc

  type: boolean

  enable = True

**Returns:** boolean status

```
LoRa.setCRC(true);

```

## setFrequency(freq)

Set the frequency.

- freq

  type: double

  902.0 - 928.0 (US-915)

**Returns:** boolean status

```
LoRa.setFrequency(902.3);

```

## setHopping(hops,hopfreqs)

Configure the frequency hopping functionality. Disable frequency hopping by setting hops to `0`.

- hops

  type: uint

  number of hops, 0 = don't use frequency hopping

- hopfreqs

  type: double[]

  the hopping frequencies, needs to contain at least 1 entry if hops > 0

**Returns:** boolean status

```
// 5 hops
LoRa.setHopping(5, [905,906,907,908,909]);
// disable hopping
LoRa.setHopping(0, []);

```

## setIQMode(iq_invert)

Enable / Disable IQ invert.

- iq_invert

  type: boolean

  enable IQ invert

```
LoRa.setIQMode(true);

```

## setPayloadLen(length)

Set payload length. If length is set to 0 the header will contain the payload length for each packet.

- length

  type: uint

  0 - 255

**Returns:** boolean status

```
LoRa.setPayloadLen(0);

```

## setPreambleLen(length)

Set the length of the preamble in symbols.

- length

  type: uint

  number of symbols: 0 - 65335

**Returns:** boolean status

```
LoRa.setPreambleLen(8);

```

## setSF(sf)

Set the spreading factor.

- sf

  type: uint

  spreading factor: 6 - 12

**Returns:** boolean status

```
LoRa.setSF(7);

```

## setSyncWord(syncword)

Set the LoRa sync word.

- syncword

  type: uint8

  sync word

```
LoRa.setSyncWord(0x42);

```

## setTxPower(level)

Set LoRa modem TX power level.

- level

  type: uint

  power level 2 - 17 (17 = PA boost).

**Returns:** boolean status

```
LoRa.setTxPower(17);

```

