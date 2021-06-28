# Util

A simple utility library.

## Methods

- [arrayEqual](#arrayequalab)
- [binToHex](#bintohexbin)
- [crc16](#crc16buf)
- [fromTwosComplement](#fromtwoscomplementtwoscomplementnumberbytes)
- [hexToBin](#hextobinhex)
- [htons](#htonsnumber)
- [reverse](#reversebin)

---

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

## crc16(buf)

calculate crc16 of the buffer

- buf

  type: bytearray

  buffer

**Returns:** uint16

```

```

## fromTwosComplement(twosComplement,numberBytes)

convert twos complement to int

- twosComplement

  type: bytearray

  twos complement

- numberBytes

  type: int

  number of bytes in towsComplement

**Returns:** int

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

## reverse(bin)

reverse the bytes in a plain buffer

- bin

  type: plainbuffer

  binary buffer

**Returns:** plain buffer

```

```

