#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include <esp_system.h>
#include "sdkconfig.h"
#include "mbedtls/cmac.h"
#include "mbedtls/aes.h"

#include <duktape.h>

#include "log.h"
#include "duk_helpers.h"

/*
 * based on: https://github.com/nkolban/duktape-esp32.git
 * license: Apache 2.0
 */

/* jsondoc
{
"class": "Crypto",
"longtext": "
Documentation for the Crypto API.

Some code is based on: https://github.com/nkolban/duktape-esp32.git
under the Apache 2.0 license.

Most of the crypto APIs use Plain Buffers (https://wiki.duktape.org/howtobuffers2x).
"
}
*/

#define AES_BLOCK_SIZE 16

/* jsondoc
{
"name": "aes128EcbEnc",
"args": [
{"name": "key", "vtype": "plain buffer", "text": "encryption key 16 bytes"},
{"name": "data", "vtype": "plain buffer", "text": "data to be encrypted, encryption will be in place and overwrite data."}
],
"text": "Encrypt using AES 128 in ECB mode.",
"return": "boolean status",
"example": "
key = Duktape.dec('hex', '2b7e151628aed2a6abf7158809cf4f3c');
data = Duktape.dec('hex', '6bc1bee22e409f96e93d7e117393172a');
Crypto.aes128EcbEnc(key, data);
"
}
*/
static duk_ret_t aes_128_ecb_encrypt(duk_context *ctx)
{
    duk_size_t keySize;
    uint8_t *key = duk_require_buffer_data(ctx, 0, &keySize);

    duk_size_t dataSize;
    uint8_t *data = duk_require_buffer_data(ctx, 1, &dataSize);

    int ret = 0;
    mbedtls_aes_context aes_ctx;

    mbedtls_aes_init(&aes_ctx);

    ret = mbedtls_aes_setkey_enc(&aes_ctx, key, 128);

    if (ret < 0)
    {
        mbedtls_aes_free(&aes_ctx);
        duk_push_boolean(ctx, 0);
        return 1;
    }

    ret = mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, data, data);

    mbedtls_aes_free(&aes_ctx);

    duk_push_boolean(ctx, ret == 0);
    return 1;
}

/* jsondoc
{
"name": "aes128EcbDec",
"args": [
{"name": "key", "vtype": "plain buffer", "text": "decryption key 16 bytes"},
{"name": "data", "vtype": "plain buffer", "text": "data to be decrypted, decryption will be in place and overwrite data."}
],
"text": "Decrypt using AES 128 in ECB mode.",
"return": "boolean status",
"example": "
key = Duktape.dec('hex', '2b7e151628aed2a6abf7158809cf4f3c');
data = Duktape.dec('hex', '6bc1bee22e409f96e93d7e117393172a');
Crypto.aes128EcbDec(key, data);
"
}
*/
static duk_ret_t aes_128_ecb_decrypt(duk_context *ctx)
{
    duk_size_t keySize;
    uint8_t *key = duk_require_buffer_data(ctx, 0, &keySize);

    duk_size_t dataSize;
    uint8_t *data = duk_require_buffer_data(ctx, 1, &dataSize);

    int ret = 0;
    mbedtls_aes_context aes_ctx;

    mbedtls_aes_init(&aes_ctx);

    ret = mbedtls_aes_setkey_dec(&aes_ctx, key, 128);

    if (ret < 0)
    {
        mbedtls_aes_free(&aes_ctx);
        duk_push_boolean(ctx, 0);
        return 1;
    }

    ret = mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_DECRYPT, data, data);

    mbedtls_aes_free(&aes_ctx);

    duk_push_boolean(ctx, ret == 0);
    return 1;
}

/* jsondoc
{
"name": "aes128CbcEnc",
"args": [
{"name": "key", "vtype": "plain buffer", "text": "encryption key 16 bytes"},
{"name": "iv", "vtype": "plain buffer", "text": "IV 16 bytes"},
{"name": "data", "vtype": "plain buffer", "text": "data to be encrypted, encryption will be in place and overwrite data. Must be be large enough to hold encrypted data."},
{"name": "data_len", "vtype": "uint", "text": "length of data"}
],
"return": "boolean status",
"text": "Encrypt using AES 128 in CBC mode.",
"example": "
"
}
*/
static duk_ret_t aes_128_cbc_encrypt(duk_context *ctx)
{
    duk_size_t keySize;
    uint8_t *key = duk_require_buffer_data(ctx, 0, &keySize);

    duk_size_t ivSize;
    uint8_t *iv = duk_require_buffer_data(ctx, 1, &ivSize);

    duk_size_t dataSize;
    uint8_t *data = duk_require_buffer_data(ctx, 2, &dataSize);

    int data_len = duk_require_int(ctx, 3);

    int ret = 0;
    mbedtls_aes_context aes_ctx;
    uint8_t cbc[AES_BLOCK_SIZE];

    mbedtls_aes_init(&aes_ctx);

    ret = mbedtls_aes_setkey_enc(&aes_ctx, key, 128);

    if (ret < 0)
    {
        mbedtls_aes_free(&aes_ctx);
        duk_push_boolean(ctx, 0);
        return 1;
    }

    memcpy(cbc, iv, AES_BLOCK_SIZE);

    ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_ENCRYPT, data_len, cbc, data, data);

    mbedtls_aes_free(&aes_ctx);

    duk_push_boolean(ctx, ret == 0);
    return 1;
}

/* jsondoc
{
"name": "aes128CbcDec",
"args": [
{"name": "key", "vtype": "plain buffer", "text": "decryption key 16 bytes"},
{"name": "iv", "vtype": "plain buffer", "text": "IV 16 bytes"},
{"name": "data", "vtype": "plain buffer", "text": "data to be decrypted, decryption will be in place and overwrite data."},
{"name": "data_len", "vtype": "uint", "text": "length of data"}
],
"return": "boolean status",
"text": "Decrypt using AES 128 in CBC mode.",
"example": "
"
}
*/
static duk_ret_t aes_128_cbc_decrypt(duk_context *ctx)
{
    duk_size_t keySize;
    uint8_t *key = duk_require_buffer_data(ctx, 0, &keySize);

    duk_size_t ivSize;
    uint8_t *iv = duk_require_buffer_data(ctx, 1, &ivSize);

    duk_size_t dataSize;
    uint8_t *data = duk_require_buffer_data(ctx, 2, &dataSize);

    int data_len = duk_require_int(ctx, 3);

    int ret = 0;
    mbedtls_aes_context aes_ctx;
    uint8_t cbc[AES_BLOCK_SIZE];

    mbedtls_aes_init(&aes_ctx);

    ret = mbedtls_aes_setkey_dec(&aes_ctx, key, 128);

    if (ret < 0)
    {
        mbedtls_aes_free(&aes_ctx);
        duk_push_boolean(ctx, 0);
        return 1;
    }

    memcpy(cbc, iv, AES_BLOCK_SIZE);

    ret = mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, data_len, cbc, data, data);

    mbedtls_aes_free(&aes_ctx);

    duk_push_boolean(ctx, ret == 0);
    return 1;
}

/* jsondoc
{
"name": "aes128Cmac",
"args": [
{"name": "key", "vtype": "plain buffer", "text": "key 16 bytes"},
{"name": "data", "vtype": "plain buffer", "text": "data to authenticate"},
{"name": "cmac", "vtype": "plain buffer", "text": "CMAC 16 bytes"}
],
"text": "Compute the AES 128 CMAC of the data.",
"return": "boolean status",
"example": "
key = Duktape.dec('hex', '2b7e151628aed2a6abf7158809cf4f3c');
data = Duktape.dec('hex', '6bc1bee22e409f96e93d7e117393172a');
output =  Uint8Array.allocPlain(16);
Crypto.aes128Cmac(key, data, output);
print(Duktape.enc('hex', output));
"
}
*/
static duk_ret_t aes_128_cmac(duk_context *ctx)
{
    duk_size_t keySize;
    uint8_t *key = duk_require_buffer_data(ctx, 0, &keySize);

    duk_size_t dataSize;
    uint8_t *data = duk_require_buffer_data(ctx, 1, &dataSize);

    duk_size_t outSize;
    uint8_t *output = duk_require_buffer_data(ctx, 2, &outSize);

    int ret = 0;

    mbedtls_cipher_info_t *cipher_info = (mbedtls_cipher_info_t *)mbedtls_cipher_info_from_type(MBEDTLS_CIPHER_AES_128_ECB);

    // key size must be in bits
    ret = mbedtls_cipher_cmac(cipher_info, key, keySize * 8, data, dataSize, output);

    duk_push_boolean(ctx, ret == 0);
    return 1;
}

/* jsondoc
{
"name": "fillRandom",
"args": [
{"name": "data", "vtype": "plain buffer", "text": "buffer to fill with random bytes"}
],
"text": "Fill buffer with random bytes from the hardware random generator.",
"example": "
output = Uint8Array.allocPlain(16);
Crypto.fillRandom(output);
print(Duktape.enc('hex', output));
"
}
*/
static duk_ret_t fill_random(duk_context *ctx)
{
    duk_size_t dataSize;
    uint8_t *data = duk_require_buffer_data(ctx, 1, &dataSize);
    esp_fill_random(data, dataSize);
    return 0;
}

static duk_function_list_entry crypto_funcs[] = {
    {"aes128CbcEnc", aes_128_cbc_encrypt, 4},
    {"aes128CbcDec", aes_128_cbc_decrypt, 4},
    {"aes128EcbEnc", aes_128_ecb_encrypt, 2},
    {"aes128EcbDec", aes_128_ecb_decrypt, 2},
    {"aes128Cmac", aes_128_cmac, 3},
    {"fillRandom", fill_random, 1},
    {NULL, NULL, 0},
};

void crypto_register(duk_context *ctx)
{
    duk_push_global_object(ctx);
    duk_push_object(ctx);

    duk_put_function_list(ctx, -1, crypto_funcs);
    duk_put_prop_string(ctx, -2, "Crypto");
    duk_pop(ctx);
}
