#include <Arduino.h>
#include "eeprom.h"

void setup() {
    Serial.begin(115200);
    eeprom_init();
}

void loop() {
    static char buf[256];
    static uint16_t address = 0;
    static uint8_t  data = 0x10;

    if (!true) {
        uint8_t oldData = eeprom_readByte(address);
        sprintf(buf, "%04x %02x", address, oldData);
        Serial.println(buf);
    }
    else {
        uint8_t oldData = eeprom_readByte(address);
        eeprom_writeByte(address, data);
        sprintf(buf, "%04x read:%02x writing:%02x", address, oldData, data);
        Serial.println(buf);
        while (1) {
            uint8_t newData = eeprom_readByte(address);
            sprintf(buf, "... read back %02x: %s", newData,
                (newData == data) ? "READY" : "not ready");
            Serial.println(buf);
            if (newData == data) {
                break;
            }
            delay(10);
        }
    }


    delay(500);

    address = (address + 1) & 0x7fff;
    data++;
}
