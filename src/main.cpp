#include <Arduino.h>
#include "eeprom_burner.h"
#include "randomiser.h"

#define ARRAY_COUNT(a)  (sizeof(a)/sizeof((a)[0]))

static void fillBuffer(uint8_t *buf, int bytes);

static uint8_t databuffer[1024];

static char buf[256];
#define PRINTF(fmt, ...)    \
    { sprintf(buf, fmt, __VA_ARGS__); Serial.println(buf); }

void setup() {
    Serial.begin(115200);
    randomiser_init(Serial);
    fillBuffer(databuffer, ARRAY_COUNT(databuffer));
    eb_init();
}

void loop() {

    int good = 0;
    int bad = 0;
    uint16_t base = random(32) << 10;

    PRINTF("Writing pages to %04x", base);
    const uint8_t pageSize = 64;
    for (uint16_t offset = 0; offset < ARRAY_COUNT(databuffer); offset += pageSize) {
        uint16_t address = base + offset;
        bool ok = eb_writePage(address, databuffer + offset, pageSize);

        if (!ok) {
            PRINTF("%04x %s", address, ok ? "ok" : "FAILED");
        }

        if (ok) {
            good++;
        }
        else {
            bad++;
        }
    }
/*
    for (uint16_t offset = 0; offset < ARRAY_COUNT(databuffer); offset++) {
        uint8_t data = databuffer[offset];
        uint16_t address = base + offset;

        uint8_t oldData = eb_readByte(address);
        bool ok = eb_writeByte(address, data);
        uint8_t newData = eb_readByte(address);

        if (!ok) {
            PRINTF("%04x read:%02x writing:%02x=%s readback:%02x",
                address, oldData, data,
                ok ? "ok" : "FAILED",
                newData);
        }

        if (ok) {
            good++;
        }
        else {
            bad++;
        }
    }
*/
    PRINTF("Done: %d good, %d bad", good, bad);

    PRINTF("Reading bytes from %04x", base);
    good = bad = 0;
    for (uint16_t offset = 0; offset < ARRAY_COUNT(databuffer); offset++) {
        uint16_t address = base + offset;
        uint8_t got = eb_readByte(address);
        uint8_t expected = databuffer[offset];
        bool ok = got == expected;

        if (!ok) {
            PRINTF("%04x read:%02x expected:%02x=%s",
                address, got, expected,
                ok ? "ok" : "FAILED");
            Serial.println(buf);
        }

        if (ok) {
            good++;
        }
        else {
            bad++;
        }
    }
    PRINTF("Done: %d good, %d bad", good, bad);

    while (1) { }
}

static void fillBuffer(uint8_t* buf, int bytes) {
    // Computes the srecord checksum
    uint8_t checksum = 0;
    for ( ; bytes != 0; bytes--) {
        uint8_t data = random(256);
        checksum += data;
        *buf++ = data;
    }
    checksum = ~checksum;
    Serial.print("Buffer checksum: ");
    Serial.println(checksum, DEC);
}
