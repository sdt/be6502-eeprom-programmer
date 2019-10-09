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

    uint16_t good = 0;
    uint16_t bad = 0;

    const uint16_t romSize = 1024u * 32u;
    const uint8_t pageSize = 64;
    PRINTF("Writing %u pages", romSize / pageSize);
    for (uint16_t address = 0; address < romSize; address += pageSize) {
        uint16_t dataOffset = address & (ARRAY_COUNT(databuffer)-1);
        bool ok = eb_writePage(address, databuffer + dataOffset, pageSize);

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
    PRINTF("Done: %u pages good, %u bad", good, bad);

    PRINTF("Reading %u bytes", romSize);
    good = bad = 0;
    for (uint16_t address = 0; address < romSize; address++) {
        uint16_t dataOffset = address & (ARRAY_COUNT(databuffer)-1);
        uint8_t got = eb_readByte(address);
        uint8_t expected = databuffer[dataOffset];
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
    PRINTF("Done: %u good, %u bad", good, bad);

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
