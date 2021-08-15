#ifndef INCLUDE_EEPROM_BURNER_H
#define INCLUDE_EEPROM_BURNER_H

#include <Arduino.h>

extern void eb_init();
extern bool eb_writePage(uint16_t address, const uint8_t* data, uint8_t size);
extern bool eb_verifyPage(uint16_t address, const uint8_t* data, uint8_t size);
extern void eb_pinTest(HardwareSerial& serial);

#endif // INCLUDE_EEPROM_BURNER_H
