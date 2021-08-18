#ifndef INCLUDE_EEPROM_BURNER_H
#define INCLUDE_EEPROM_BURNER_H

#include <Arduino.h>

enum ebError {
    ebError_OK = 0,
    ebError_PageBoundaryCrossed,
    ebError_WriteCompletionDataMismatch,
    ebError_WriteCompletionTimeout,
};

extern void eb_init();
extern ebError eb_writePage(uint16_t address, const uint8_t* data, uint8_t size);
extern bool eb_verifyPage(uint16_t address, const uint8_t* data, uint8_t size);
extern void eb_pinTest(HardwareSerial& serial);
extern const char* eb_errorMessage(ebError error);

#endif // INCLUDE_EEPROM_BURNER_H
