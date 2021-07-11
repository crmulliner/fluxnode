# Util

A simple utility library.

## Methods

- [arrayEqual](#arrayequalab)
- [binToHex](#bintohexbin)
- [copyIndexLen](#copyindexlennumbernumbernumbernumbernumber)
- [crc16](#crc16buf)
- [fromTwosComplement](#fromtwoscomplementtwoscomplementnumberbytes)
- [hexToBin](#hextobinhex)
- [hton[s|l]](#hton[s|l]number)
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

## copyIndexLen(number,number,number,number,number)

copy len bytes from src at src index to dst at dst index

- number

  type: array

  src

- number

  type: int

  src index

- number

  type: array

  dst

- number

  type: int

  dst index

- number

  type: int

  len

**Returns:** 

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

## hton[s|l](number)

convert host to network byte order (uint16/32)

- number

  type: uint16/32

  uint16/32

**Returns:** uint16/32

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

