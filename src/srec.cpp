#include "srec.h"
#include <stdlib.h>

struct SRecord {
    char header[2];
    char byteCount[2];
    char address[4];
    char data[0];
};

static uint8_t hexChar(char hex) {
    if (hex >= '0' && hex <= '9') {
        return hex - '0';
    }
    if (hex >= 'A' && hex <= 'F') {
        return hex - 'A' + 10;
    }
    return 0xff;
}

static uint8_t read8(const char* buf) {
    return (hexChar(buf[0]) << 4) | (hexChar(buf[1]));
}

static void bswap(uint16_t* p) {
    uint8_t* hp = (uint8_t*) p;
    uint8_t t = hp[0];
    hp[0] = hp[1];
    hp[1] = t;
}

SRec1* parseSRec1(char* buffer, uint16_t size) {

    SRecord* in = (SRecord*) buffer;

    // Check the header makes sense.
    if ((in->header[0] != 'S') || (in->header[1] != '1')) {
        return NULL;
    }

    // Convert the data in-place. The SRec1 will always be smaller than the
    // original SRecord, so this fits.
    SRec1* out = (SRec1*) buffer;

    // Before we go running off the end of crazyland, check that the data size
    // makes sense. size = dataSize * 2 + (2 + 2 + 4 + 2)
    uint8_t byteCount = read8(in->byteCount);
    if (byteCount * 2u + 4u != size) {
        return NULL;
    }

    // Compute the checksum and convert the data in one pass.
    uint8_t checksum = 0;
    char* src = buffer + 2;
    uint8_t* dest = &(out->dataSize);
    uint16_t n = byteCount; // plus one for dataSize, less one for checksum
    for (uint16_t i = 0; i < n; i++) {
        uint8_t value = read8(&src[i * 2]);
        checksum += value;
        dest[i] = value;
    }

    // XOR the final checksum value with the declared one. It should be zero.
    checksum ^= read8(&src[n * 2]);
    if (checksum != 0xff) {
        return NULL;
    }

    // The fields will already have been converted. We just need to fix up
    // the address & dataSize.
    bswap(&out->address);
    out->dataSize -= 3;

    return out;
}


