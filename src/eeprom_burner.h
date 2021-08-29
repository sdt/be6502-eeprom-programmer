#ifndef INCLUDE_EEPROM_BURNER_H
#define INCLUDE_EEPROM_BURNER_H

#include <Arduino.h>

enum ebError {
    ebError_OK = 0,
    ebError_PageBoundaryCrossed,
    ebError_WriteCompletionDataMismatch,
    ebError_WriteCompletionTimeout,
    ebError_OutOfSession,
};

extern void eb_init();
extern void eb_beginSession();
extern void eb_endSession(bool doReset);

extern ebError eb_chipErase();
extern ebError eb_writePage(uint16_t address, const uint8_t* data, uint8_t size);
extern bool eb_verifyPage(uint16_t address, const uint8_t* data, uint8_t size);

extern const char* eb_errorMessage(ebError error);

extern void eb_pinTest(HardwareSerial& serial);

#endif // INCLUDE_EEPROM_BURNER_H
