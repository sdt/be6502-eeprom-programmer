#include "eeprom_burner.h"

// PF: A0-A7
// PK: A8-A14
// PL: D0-D7
// PH4: ~CS
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

#define BIT(n)  (1<<(n))

const uint8_t Control_CS = BIT(4);
const uint8_t Control_OE = BIT(5);
const uint8_t Control_WE = BIT(6);
const uint8_t CS_on  = 0;
const uint8_t CS_off = Control_CS;
const uint8_t OE_on  = 0;
const uint8_t OE_off = Control_OE;
const uint8_t WE_on  = 0;
const uint8_t WE_off = Control_WE;
const uint8_t Control_Mask = Control_CS | Control_OE | Control_WE;
const uint8_t Data_Write = 0xff;
const uint8_t Data_Read  = 0x00;

static void setControlBits(uint8_t bits) {
    PORT_OUT(CONTROL) = (PORT_OUT(CONTROL) & ~Control_Mask) | bits;
}

static void setAddress(uint16_t address) {
    uint8_t addr_low  = address & 0x00ff;
    uint8_t addr_high = address >> 8;

    PORT_OUT(ADDR_LOW)  = addr_low;
    PORT_OUT(ADDR_HIGH) = addr_high;
}

void eb_init() {
    PORT_DIR(ADDR_LOW)  = 0xff; // A0-A7 output pins
    PORT_OUT(ADDR_LOW)  = 0x00;

    PORT_DIR(ADDR_HIGH) = 0x7f; // A8-A15 output pins
    PORT_OUT(ADDR_HIGH) = 0x00;

    PORT_DIR(DATA)      = Data_Read;
    PORT_OUT(DATA)      = 0x00; // data is input

    PORT_DIR(CONTROL)   = Control_Mask;
    setControlBits(CS_on | OE_off | WE_off);
}

uint8_t eb_readByte(uint16_t address) {
    setAddress(address);
    setControlBits(CS_on | OE_on | WE_off);
    NOP; NOP; NOP; // tACC = 150
    PORT_DIR(DATA) = Data_Read;
    uint8_t data = PORT_IN(DATA);
    setControlBits(CS_on | OE_off | WE_off);
    return data;
}

void eb_writeByte(uint16_t address, uint8_t data) {
    setAddress(address);
    setControlBits(CS_on | OE_off | WE_off);
    PORT_DIR(DATA) = Data_Write;
    PORT_OUT(DATA) = data;
    setControlBits(CS_on | OE_off | WE_on); // WE -> on, latch addr
    NOP; NOP; // tWP = 100
    setControlBits(CS_on | OE_off | WE_off); // WE -> off, latch data
    NOP; NOP; // tWP = 50
    PORT_DIR(DATA) = Data_Read;
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

void eb_pinTest(HardwareSerial& serial) {
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

