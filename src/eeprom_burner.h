#ifndef INCLUDE_EEPROM_BURDER_H
#define INCLUDE_EEPROM_BURDER_H

#include <Arduino.h>

extern void eb_init();
extern uint8_t eb_readByte(uint16_t address);
extern bool eb_writeByte(uint16_t address, uint8_t data);
extern void eb_pinTest(HardwareSerial& serial);

#endif // INCLUDE_EEPROM_BURDER_H
