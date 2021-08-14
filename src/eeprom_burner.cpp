#include "eeprom_burner.h"

#define BIT(n)                      (1<<(n))
#define _PASTE(A, B)        A##B
#define PORT_OUT(port_id)           _PASTE(PORT, port_id)
#define PORT_IN(port_id)            _PASTE(PIN, port_id)
#define PORT_DIR(port_id)           _PASTE(DDR, port_id)
#define SET_PORT_BIT(port, mask)    (PORT_OUT(port) |= (mask))
#define CLEAR_PORT_BIT(port, mask)  (PORT_OUT(port) &= ~(mask))

#define NOP __asm__ __volatile__ ("nop\n\t") // 65 ns @ 16 Mhz

#if defined(ARDUINO_AVR_MEGA2560)

    // PF: A0-A7
    // PK: A8-A14
    // PL: D0-D7
    // PH4: ~CS
    // PH5: ~OE
    // PH6: ~WE
    #define ADDR_LOW            F
    #define ADDR_HIGH           K
    #define DATA                L
    #define CONTROL             H

    const uint8_t Control_CS = BIT(4);
    const uint8_t Control_OE = BIT(5);
    const uint8_t Control_WE = BIT(6);
    const uint8_t Control_Mask = Control_CS | Control_OE | Control_WE;
    const uint8_t Data_Write = 0xff;
    const uint8_t Data_Read  = 0x00;

    static void initPins() {
        PORT_DIR(ADDR_LOW)  = 0xff; // A0-A7 output pins
        PORT_OUT(ADDR_LOW)  = 0x00;

        PORT_DIR(ADDR_HIGH) = 0x7f; // A8-A15 output pins
        PORT_OUT(ADDR_HIGH) = 0x00;

        PORT_DIR(DATA)      = Data_Read;
        PORT_OUT(DATA)      = 0x00; // data is input

        PORT_DIR(CONTROL)   = Control_Mask;
    }

    static void setAddress(uint16_t address) {
        uint8_t addr_low  = address & 0x00ff;
        uint8_t addr_high = address >> 8;

        PORT_OUT(ADDR_LOW)  = addr_low;
        PORT_OUT(ADDR_HIGH) = addr_high;
    }

    static uint8_t readData() {
        return PORT_IN(DATA);
    }

    static void writeData(uint8_t data) {
        PORT_OUT(DATA) = data;
    }

    static void setDataReadMode() {
        PORT_DIR(DATA) = Data_Read;
    }

    static void setDataWriteMode() {
        PORT_DIR(DATA) = Data_Write;
    }

    static void chipSelectOn()  { CLEAR_PORT_BIT(CONTROL, Control_CS); }
    static void chipSelectOff() { SET_PORT_BIT(CONTROL,   Control_CS); }

#elif defined(ARDUINO_AVR_NANO)

    static void initPins() { }
    static void setAddress(uint16_t address) { }
    static uint8_t readData() { return 0; }
    static void writeData(uint8_t data) { }
    static void setDataReadMode() { }
    static void setDataWriteMode() { }

    static void chipSelectOn()      { }
    static void chipSelectOff()     { }

    #define CONTROL C
    const uint8_t Control_OE = BIT(4);
    const uint8_t Control_WE = BIT(5);
#else
    #error "Building for UNKNOWN"
#endif

const uint8_t Page_Bits = 6;
const uint8_t Page_Size = 1 << Page_Bits;

static bool waitForWriteCompletion(uint8_t expectedData);

// All three control lines are active low.
static void outputEnableOn()    { CLEAR_PORT_BIT(CONTROL, Control_OE); }
static void outputEnableOff()   { SET_PORT_BIT(CONTROL,   Control_OE); }

static void writeEnableOn()     { CLEAR_PORT_BIT(CONTROL, Control_WE); }
static void writeEnableOff()    { SET_PORT_BIT(CONTROL,   Control_WE); }

void eb_init() {
    initPins();
    chipSelectOff();
    outputEnableOff();
    writeEnableOff();
}

uint8_t eb_readByte(uint16_t address) {
    setAddress(address);
    chipSelectOn();
    outputEnableOn();

    NOP; NOP; NOP; // tACC = 150ns, tCE = 150ns, tOE = 70

    uint8_t data = readData();

    outputEnableOff();
    chipSelectOff();

    NOP; // tDF = 50ns

    return data;
}

bool eb_writeByte(uint16_t address, uint8_t data) {
    setAddress(address);
    setDataWriteMode();
    writeData(data);

    // Turn chip select on here at the start, and turn it off again after
    // waitForWriteCompletion.
    chipSelectOn();
    writeEnableOn();    // falling edge latches address

    NOP; NOP;           // tWP = 100

    writeEnableOff();   // rising edge latches data

    NOP;                // tWPH = 50

    setDataReadMode();

    return waitForWriteCompletion(data);
}

bool eb_writePage(uint16_t address, const uint8_t* data, uint8_t size) {
    if (size == 0) {
        return true;
    }

    uint16_t startPage = address >> Page_Bits;
    uint16_t endPage   = (address + size - 1) >> Page_Bits;

    if (startPage != endPage) {
        return false;
    }

    chipSelectOn();
    setDataWriteMode();
    for (uint8_t offset = 0; offset < size; offset++) {

        setAddress(address + offset);
        writeData(data[offset]);

        writeEnableOn();    // falling edge latches address

        NOP; NOP;           // tWP = 100

        writeEnableOff();   // rising edge latches data

        NOP;                // tWPH = 50
    }
    setDataReadMode();

    return waitForWriteCompletion(data[size - 1]);
}

bool eb_verifyPage(uint16_t address, const uint8_t* data, uint8_t size) {
    for (uint8_t offset = 0; offset < size; offset++) {
        if (eb_readByte(address + offset) != data[offset]) {
            return false;
        }
    }
    return true;
}


static bool waitForWriteCompletion(uint8_t expectedData) {

    const long maxRetries = 100000;

    // On entry, chip select is on, output and write enable are off.
    // DATA port is set to input.
    outputEnableOn();
    NOP; NOP;
    uint8_t prevData = readData();
    outputEnableOff();
    NOP; NOP;

    for (long attempt = 0; attempt < maxRetries; attempt++) {
        outputEnableOn();
        NOP; NOP;
        uint8_t nextData = readData();
        outputEnableOff();
        NOP; NOP;

        if (prevData == nextData) {
            chipSelectOff();
            return nextData == expectedData;
        }

        prevData = nextData;
    }

    // Timed out
    chipSelectOff();
    return false;
}

static void waitForKey(HardwareSerial& serial) {
    while (!serial.available()) { }
    while (serial.available()) {
        serial.read();
        delay(1);
    }
}

#if defined(ARDUINO_AVR_MEGA2560)

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

#elif defined(ARDUINO_AVR_NANO)
    void eb_pinTest(HardwareSerial& serial) {
        serial.println("Woo!");
        waitForKey(serial);
    }
#endif
