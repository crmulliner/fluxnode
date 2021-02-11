/*
 * Copyright: Collin Mulliner
 */

function OnStart() {
    print("------ cryptotest.js ------\n");

    var key = Duktape.dec('hex', '2b7e151628aed2a6abf7158809cf4f3c');
    var data = Duktape.dec('hex', '6bc1bee22e409f96e93d7e117393172a');
    var ciphertext = Duktape.dec('hex', '3AD77BB40D7A3660A89ECAF32466EF97');
    Crypto.aes128EcbEnc(key, data);
    
    print("aes128EcbEnc: ");
    if (!arrayEqual(data,ciphertext)) {
        print("failed\n");
    }
    else {
        print("success\n");
    }

    ciphertext = Duktape.dec('hex', '3AD77BB40D7A3660A89ECAF32466EF97');
    data = Duktape.dec('hex', '6bc1bee22e409f96e93d7e117393172a');
    key = Duktape.dec('hex', '2b7e151628aed2a6abf7158809cf4f3c');
    Crypto.aes128EcbDec(key, ciphertext);

    print("aes128EcbDec: ");
    if (!arrayEqual(data,ciphertext)) {
        print("failed\n");
    }
    else {
        print("success\n");
    }

    key = Duktape.dec('hex', '2b7e151628aed2a6abf7158809cf4f3c');
    data = Duktape.dec('hex', '6bc1bee22e409f96e93d7e117393172a');
    var cmac = Duktape.dec('hex', '070a16b46b4d4144f79bdd9dd04a287c');
    var output = Uint8Array.allocPlain(16);
    Crypto.aes128Cmac(key, data, output);
    
    print("aes128Cmac: ");
    if (!arrayEqual(output,cmac)) {
        print("failed\n");
    }
    else {
        print("success\n");
    }

    print("random numbers\n");
    var i = 0;
    for (i = 0; i < 10; i++) {
        print("rand: " + Math.random() + "\n");
    }
}

function OnEvent(event) {
}

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
