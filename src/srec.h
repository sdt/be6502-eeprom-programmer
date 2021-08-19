#ifndef INCLUDE_SREC_H
#define INCLUDE_SREC_H

#include <stdint.h>

struct SRec1 {
    char header[2];
    uint8_t dataSize;
    uint16_t address;
    uint8_t data[252];
};

// If buffer is a valid S1 record, returns pointer to SRec1, otherwise NULL.
// The buffer is overwritten with the contents of SRec1. SRec1 is always smaller
// than the input buffer.
extern SRec1* parseSRec1(char* buffer, uint16_t size);

// Input record will be a maximum of 2 + 2 + 255 * 2 = 514 bytes
const uint16_t c_srecBufferSize = 515; // allow space for a newline

#endif // INCLUDE_SREC_H
