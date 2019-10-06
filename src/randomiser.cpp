#include "randomiser.h"

#include <Arduino.h>
#include <EEPROM.h>

const int EEPROM_ADDRESS = 0;

void randomiser_init(HardwareSerial& serial) {
    uint8_t seed = EEPROM.read(EEPROM_ADDRESS);
    serial.print("Setting random seed: ");
    serial.println(seed);
    randomSeed(seed);
    EEPROM.write(EEPROM_ADDRESS, seed+1);
}
