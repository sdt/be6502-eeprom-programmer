#include "eeprom.h"

// PF: A0-A7
// PK: A8-A14
// PL: D0-D7
// PH4: ~CE
// PH5: ~OE
// PH6: ~WE

#define _PASTE(A, B)        A##B

#define PORT_OUT(PORT_ID)   _PASTE(PORT, PORT_ID)
#define PORT_IN(PORT_ID)    _PASTE(PIN, PORT_ID)
#define PORT_DIR(PORT_ID)   _PASTE(DDR, PORT_ID)

#define ADDR_LOW            F
#define ADDR_HIGH           K
#define DATA                L
#define CONTROL             H

// 65 ns @ 16 Mhz
#define NOP __asm__ __volatile__ ("nop\n\t")

void eeprom_init() {
    PORT_OUT(ADDR_LOW)  = 0x00;
    PORT_DIR(ADDR_LOW)  = 0xff;  // A0-A7 output pins

    PORT_OUT(ADDR_HIGH) = 0x00;
    PORT_DIR(ADDR_HIGH) = 0x7f;  // A8-A15 output pins

    PORT_OUT(DATA)      = 0x00;
    PORT_DIR(DATA)      = 0x00;

    PORT_OUT(CONTROL)   = 0x38;
    PORT_DIR(CONTROL)   = 0x38;
}

uint8_t eeprom_readByte(uint16_t address) {
    PORT_OUT(ADDR_LOW) = address & 0xff;
    // tAA = 55/70/85 ns
    NOP; NOP;
    return PORT_IN(DATA);
}

static void waitForKey(HardwareSerial& serial) {
    while (!serial.available()) { }
    while (serial.available()) {
        serial.read();
        delay(1);
    }
}

#define SET_PIN(number, port, bit)  \
    PORT_DIR(port) |= 1<<(bit); \
    PORT_OUT(port) |= 1<<(bit); \
    serial.println("Pin " #number); \
    waitForKey(serial); \
    PORT_OUT(port) &= ~(1<<(bit));

#define SKIP_PIN(number, func) \
    serial.println("Pin " #number " is " func); \
    waitForKey(serial);

void eeprom_pinTest(HardwareSerial& serial) {
    SET_PIN(1,   ADDR_HIGH, 14-8);
    SET_PIN(2,   ADDR_HIGH, 12-8);
    SET_PIN(3,   ADDR_LOW,  7);
    SET_PIN(4,   ADDR_LOW,  6);
    SET_PIN(5,   ADDR_LOW,  5);
    SET_PIN(6,   ADDR_LOW,  4);
    SET_PIN(7,   ADDR_LOW,  3);
    SET_PIN(8,   ADDR_LOW,  2);
    SET_PIN(9,   ADDR_LOW,  1);
    SET_PIN(10,  ADDR_LOW,  0);
    SET_PIN(11,  DATA,      0);
    SET_PIN(12,  DATA,      1);
    SET_PIN(13,  DATA,      2);
    SKIP_PIN(14, "ground");

    SET_PIN(15,  DATA,      3);
    SET_PIN(16,  DATA,      4);
    SET_PIN(17,  DATA,      5);
    SET_PIN(18,  DATA,      6);
    SET_PIN(19,  DATA,      7);
    SET_PIN(20,  CONTROL,   4);
    SET_PIN(21,  ADDR_HIGH, 10-8);
    SET_PIN(22,  CONTROL,   5);
    SET_PIN(23,  ADDR_HIGH, 11-8);
    SET_PIN(24,  ADDR_HIGH, 9-8);
    SET_PIN(25,  ADDR_HIGH, 8-8);
    SET_PIN(26,  ADDR_HIGH, 13-8);
    SET_PIN(27,  CONTROL,   6);
    SKIP_PIN(28,  "+5V");
}

