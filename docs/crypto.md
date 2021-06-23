# Crypto

Documentation for the Crypto API.

Some code is based on: https://github.com/nkolban/duktape-esp32.git
under the Apache 2.0 license.

Most of the crypto APIs use Plain Buffers (https://wiki.duktape.org/howtobuffers2x).

## Methods

- [aes128CbcDec](#aes128cbcdeckeyivdatadata_len)
- [aes128CbcEnc](#aes128cbcenckeyivdatadata_len)
- [aes128Cmac](#aes128cmackeydatacmac)
- [aes128EcbDec](#aes128ecbdeckeydata)
- [aes128EcbEnc](#aes128ecbenckeydata)
- [fillRandom](#fillrandomdata)

---

## aes128CbcDec(key,iv,data,data_len)

Decrypt using AES 128 in CBC mode.

- key

  type: plain buffer

  decryption key 16 bytes

- iv

  type: plain buffer

  IV 16 bytes

- data

  type: plain buffer

  data to be decrypted, decryption will be in place and overwrite data.

- data_len

  type: uint

  length of data

**Returns:** boolean status

```

```

## aes128CbcEnc(key,iv,data,data_len)

Encrypt using AES 128 in CBC mode.

- key

  type: plain buffer

  encryption key 16 bytes

- iv

  type: plain buffer

  IV 16 bytes

- data

  type: plain buffer

  data to be encrypted, encryption will be in place and overwrite data. Must be be large enough to hold encrypted data.

- data_len

  type: uint

  length of data

**Returns:** boolean status

```

```

## aes128Cmac(key,data,cmac)

Compute the AES 128 CMAC of the data.

- key

  type: plain buffer

  key 16 bytes

- data

  type: plain buffer

  data to authenticate

- cmac

  type: plain buffer

  CMAC 16 bytes

**Returns:** boolean status

```
key = Duktape.dec('hex', '2b7e151628aed2a6abf7158809cf4f3c');
data = Duktape.dec('hex', '6bc1bee22e409f96e93d7e117393172a');
output =  Uint8Array.allocPlain(16);
Crypto.aes128Cmac(key, data, output);
print(Duktape.enc('hex', output));

```

## aes128EcbDec(key,data)

Decrypt using AES 128 in ECB mode.

- key

  type: plain buffer

  decryption key 16 bytes

- data

  type: plain buffer

  data to be decrypted, decryption will be in place and overwrite data.

**Returns:** boolean status

```
key = Duktape.dec('hex', '2b7e151628aed2a6abf7158809cf4f3c');
data = Duktape.dec('hex', '6bc1bee22e409f96e93d7e117393172a');
Crypto.aes128EcbDec(key, data);

```

## aes128EcbEnc(key,data)

Encrypt using AES 128 in ECB mode.

- key

  type: plain buffer

  encryption key 16 bytes

- data

  type: plain buffer

  data to be encrypted, encryption will be in place and overwrite data.

**Returns:** boolean status

```
key = Duktape.dec('hex', '2b7e151628aed2a6abf7158809cf4f3c');
data = Duktape.dec('hex', '6bc1bee22e409f96e93d7e117393172a');
Crypto.aes128EcbEnc(key, data);

```

## fillRandom(data)

Fill buffer with random bytes from the hardware random generator.

- data

  type: plain buffer

  buffer to fill with random bytes

```
output = Uint8Array.allocPlain(16);
Crypto.fillRandom(output);
print(Duktape.enc('hex', output));

```

