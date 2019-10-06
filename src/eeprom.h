#ifndef INCLUDE_EEPROM_H
#define INCLUDE_EEPROM_H

#include <Arduino.h>

extern void eeprom_init();
extern uint8_t eeprom_readByte(uint16_t address);
extern void eeprom_writeByte(uint16_t address, uint8_t data);
extern void eeprom_pinTest(HardwareSerial& serial);

#endif // INCLUDE_EEPROM_H
