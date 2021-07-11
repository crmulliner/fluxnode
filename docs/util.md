# Util

A simple utility library.

## Methods

- [arrayEqual](#arrayequalab)
- [binToHex](#bintohexbin)
- [copyIndexLen](#copyindexlennumbernumbernumbernumbernumber)
- [crc16](#crc16buf)
- [fromTwosComplement](#fromtwoscomplementtwoscomplementnumberbytes)
- [hexToBin](#hextobinhex)
- [htonl](#htonlnumber)
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

## htonl(number)

convert host to network byte order (uint32)

- number

  type: uint32

  uint32

**Returns:** uint32

```

```

## htons(number)

convert host to network byte order (uint16)

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

