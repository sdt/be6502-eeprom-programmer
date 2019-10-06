#include <Arduino.h>
#include "eeprom_burner.h"
#include "randomiser.h"

#define ARRAY_COUNT(a)  (sizeof(a)/sizeof((a)[0]))

static void fillBuffer(uint8_t *buf, int bytes);

static uint8_t databuffer[1024];

static char buf[256];

void setup() {
    Serial.begin(115200);
    randomiser_init(Serial);
    fillBuffer(databuffer, ARRAY_COUNT(databuffer));
    eb_init();
}

void loop() {
    int good = 0;
    int bad = 0;
    for (uint16_t address = 0; address < ARRAY_COUNT(databuffer); address++) {
        uint8_t data = databuffer[address];

        uint8_t oldData = eb_readByte(address);
        bool ok = eb_writeByte(address, data);
        uint8_t newData = eb_readByte(address);

        sprintf(buf, "%04x read:%02x writing:%02x=%s readback:%02x",
            address, oldData, data,
            ok ? "ok" : "FAILED",
            newData);
        Serial.println(buf);

        if (ok) {
            good++;
        }
        else {
            bad++;
        }

//        delay(500);
    }
    Serial.println("Done");
    Serial.print("Good: "); Serial.println(good);
    Serial.print("Bad: "); Serial.println(bad);
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
