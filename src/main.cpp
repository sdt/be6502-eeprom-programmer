#include <Arduino.h>
#include "eeprom.h"

void setup() {
    Serial.begin(115200);
    eeprom_init();
}

void loop() {
    eeprom_pinTest(Serial);
}
