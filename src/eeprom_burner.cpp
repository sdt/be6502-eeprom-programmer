#include "eeprom_burner.h"

#define BIT(n)                          (1<<(n))
#define _PASTE(A, B)                    A##B
#define PORT_OUT(port_id)               _PASTE(PORT, port_id)
#define PORT_IN(port_id)                _PASTE(PIN, port_id)
#define PORT_DIR(port_id)               _PASTE(DDR, port_id)
#define SET_PORT_BIT(port, mask)        (PORT_OUT(port) |= (mask))
#define CLEAR_PORT_BIT(port, mask)      (PORT_OUT(port) &= ~(mask))
#define WRITE_MASKED(reg, value, mask)  (reg) = ((reg) & ~(mask)) | (value)

#define NOP __asm__ __volatile__ ("nop\n\t") // 65 ns @ 16 MHz

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
        SET_PORT_BIT(CONTROL, Control_Mask);
    }

    static void setAddress(uint16_t address) {
        uint8_t addrLow  = address & 0x00ff;
        uint8_t addrHigh = address >> 8;

        PORT_OUT(ADDR_LOW)  = addrLow;
        PORT_OUT(ADDR_HIGH) = addrHigh;
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

    static void setChipSelect(bool chipSelectOn, uint16_t) {
        if (chipSelectOn) {
            CLEAR_PORT_BIT(CONTROL, Control_CS);
        }
        else {
            SET_PORT_BIT(CONTROL, Control_CS);
        }
    }

#elif defined(ARDUINO_AVR_NANO)

    #include <avr/io.h>

    // Data pins
    // PB0-1:   D0-D1
    // PD2-7:   D2-D7

    // Address pins on SR1/SR2
    //
    // Shift register control
    // PB2:     SS (must be output)
    // PB3:     MOSI -> SER1 & SER2
    // PB4:     MISO (input, but unconnected)
    // PB5:     SCK -> SRCLK1 & SRCLK2
    //
    // PC2: OEB1 & OEB2
    // PC3: RCLK1
    // PC4: RCLK2
    //
    // PC0: ~WE
    // PC1: ~OE
    // ~CS is on A15

    const uint8_t PB0_Data0     = BIT(0);   // input
    const uint8_t PB1_Data1     = BIT(1);   // input
    const uint8_t PB2_SS        = BIT(2);   // output
    const uint8_t PB3_MOSI      = BIT(3);   // output
    const uint8_t PB4_MISO      = BIT(4);   // input-pullup
    const uint8_t PB5_SCK       = BIT(5);   // output
    const uint8_t PB_DataMask   = PB0_Data0 | PB1_Data1;
    const uint8_t PB_MASK       = PB_DataMask | PB2_SS | PB3_MOSI
                                | PB4_MISO | PB5_SCK;

    const uint8_t PC0_ROM_WEB   = BIT(0);   // output
    const uint8_t PC1_ROM_OEB   = BIT(1);   // output
    const uint8_t PC2_SR_OEB    = BIT(2);   // output
    const uint8_t PC3_RCLK1     = BIT(3);   // output
    const uint8_t PC4_RCLK2     = BIT(4);   // output
    const uint8_t PC_MASK       = PC0_ROM_WEB | PC1_ROM_OEB | PC2_SR_OEB
                                | PC3_RCLK1 | PC4_RCLK2;

    const uint8_t PD2_Data2     = BIT(2);   // input
    const uint8_t PD3_Data3     = BIT(3);   // input
    const uint8_t PD4_Data4     = BIT(4);   // input
    const uint8_t PD5_Data5     = BIT(5);   // input
    const uint8_t PD6_Data6     = BIT(6);   // input
    const uint8_t PD7_Data7     = BIT(7);   // input
    const uint8_t PD_DataMask   = PD2_Data2 | PD3_Data3 | PD4_Data4
                                | PD5_Data5 | PD6_Data6 | PD7_Data7;
    const uint8_t PD_MASK       = PD_DataMask;

    static uint8_t spiSend(uint8_t data) {
        SPDR = data;
        while ((SPSR & BIT(SPIF)) == 0) {
            // wait for SPIF
        }
        return data;
    }

    static uint8_t s_chipSelectMask = 0;
    static uint8_t s_prevAddrHigh = 0xff;
    static void setAddress(uint16_t address) {
        uint8_t addrLow  = address & 0x00ff;
        uint8_t addrHigh = (address >> 8) | s_chipSelectMask;

        uint8_t rclk;

        if (s_prevAddrHigh == addrHigh) {
            // High bits don't change, so no need to set them.
            spiSend(addrLow);

            // Only strobe the low SR
            rclk = PC3_RCLK1;
        }
        else {
            // High byte first
            spiSend(addrHigh);
            spiSend(addrLow);

            // Strobe both SRs
            rclk = PC3_RCLK1 | PC4_RCLK2;

            s_prevAddrHigh = addrHigh;
        }

        // Strobe the RCLK pins.
        SET_PORT_BIT(C, rclk);
        CLEAR_PORT_BIT(C, rclk);
    }

    static uint8_t readData() {
        return (PORT_IN(B) & PB_DataMask)
             | (PORT_IN(D) & PD_DataMask);
    }

    static void writeData(uint8_t data) {
        WRITE_MASKED(PORT_OUT(B), data & PB_DataMask, PB_DataMask);
        WRITE_MASKED(PORT_OUT(D), data & PD_DataMask, PD_DataMask);
    }

    static void setDataReadMode() {
        WRITE_MASKED(PORT_DIR(B), 0, PB_DataMask);
        WRITE_MASKED(PORT_DIR(D), 0, PD_DataMask);
    }

    static void setDataWriteMode() {
        WRITE_MASKED(PORT_DIR(B), PB_DataMask, PB_DataMask);
        WRITE_MASKED(PORT_DIR(D), PD_DataMask, PD_DataMask);
    }

    static void setChipSelect(bool chipSelectOn, uint16_t address) {
        // In the standalone circuit, CS is active low.
        // When we integrate this into the be6502 board, the EEPROM is mapped
        // to the high 32k, so A15 needs to be high.
        s_chipSelectMask = chipSelectOn ? 0x00 : 0x80;
        setAddress(address);
    }

    static void initPins() {
        WRITE_MASKED(PORT_DIR(B), PB2_SS | PB3_MOSI | PB5_SCK, PB_MASK);

        // Set MISO to input-pullup. Clear SCK.
        WRITE_MASKED(PORT_OUT(B), PB4_MISO, PB4_MISO | PB5_SCK);

        WRITE_MASKED(PORT_DIR(C), PC_MASK, PC_MASK);

        // Set PC0_ROM_WEB & PC1_ROM_OEB high. Other pins low.
        WRITE_MASKED(PORT_OUT(C), PC0_ROM_WEB | PC1_ROM_OEB, PC_MASK);

        WRITE_MASKED(PORT_DIR(D), 0, PD_MASK);

        // SPI mode zero, master, MSB first.
        #define SPI_4MHz   0
        #define SPI_1MHz   BIT(SPR0)
        #define SPI_256kHz BIT(SPR1)
        #define SPI_128kHz (BIT(SPR1) | BIT(SPR0))
        // 4MHz works, but given the rest of the project only runs at 1Mhz,
        // it feels a bit safer to stick to 1Mhz. The runttime difference is
        // pretty small.
        SPCR = BIT(SPE) | BIT(MSTR) | SPI_1MHz;

        setChipSelect(false, 0);
    }

    #define CONTROL C
    const uint8_t Control_OE = BIT(1);
    const uint8_t Control_WE = BIT(0);
#else
    #error "Building for UNKNOWN"
#endif

const uint8_t Page_Bits = 6;
const uint8_t Page_Size = 1 << Page_Bits;

static ebError waitForWriteCompletion(uint8_t expectedData);

// All three control lines are active low.
static void outputEnableOn()    { CLEAR_PORT_BIT(CONTROL, Control_OE); }
static void outputEnableOff()   { SET_PORT_BIT(CONTROL,   Control_OE); }

static void writeEnableOn()     { CLEAR_PORT_BIT(CONTROL, Control_WE); }
static void writeEnableOff()    { SET_PORT_BIT(CONTROL,   Control_WE); }

void eb_init() {
    initPins();
}

ebError eb_writePage(uint16_t address, const uint8_t* data, uint8_t size) {
    if (size == 0) {
        return ebError_OK;
    }

    uint16_t startPage = address >> Page_Bits;
    uint16_t endPage   = (address + size - 1) >> Page_Bits;

    if (startPage != endPage) {
        return ebError_PageBoundaryCrossed;
    }

    setChipSelect(true, address);
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
    setChipSelect(true, address);
    outputEnableOn();

    bool ok = true;

    for (uint8_t offset = 0; offset < size; offset++) {
        setAddress(address + offset);

        NOP; NOP; NOP; // tACC = 150ns, tCE = 150ns, tOE = 70

        uint8_t byteRead = readData();

        if (byteRead != data[offset]) {
            ok = false;
            break;
        }

        NOP; // tDF = 50ns
    }

    outputEnableOff();
    setChipSelect(false, 0);

    return ok;
}

const char* eb_errorMessage(ebError error) {
    switch (error) {

    case ebError_OK:
        return "OK";

    case ebError_PageBoundaryCrossed:
        return "page boundary crossed";

    case ebError_WriteCompletionDataMismatch:
        return "write completion data mismatch";

    case ebError_WriteCompletionTimeout:
        return "write completion timeout";

    default:
        return "unknown error code";
    }
}

static ebError waitForWriteCompletion(uint8_t expectedData) {

    const long maxRetries = 100000;

    // On entry, chip select is on, output and write enable are off.
    // DATA port is set to input.
    outputEnableOn();
    NOP; NOP;
    uint8_t prevData = readData();
    outputEnableOff();
    NOP; NOP;

    ebError ret = ebError_WriteCompletionTimeout;

    for (long attempt = 0; attempt < maxRetries; attempt++) {
        outputEnableOn();
        NOP; NOP;
        uint8_t nextData = readData();
        outputEnableOff();
        NOP; NOP;

        if (prevData == nextData) {
            ret = (nextData == expectedData)
                ? ebError_OK : ebError_WriteCompletionDataMismatch;
            break;
        }

        prevData = nextData;
    }

    setChipSelect(false, 0);
    return ret;
}

static void waitForKey(HardwareSerial& serial) {
    while (!serial.available()) { }
    while (serial.available()) {
        serial.read();
        delay(1);
    }
}

#define SKIP_PIN(number, func) \
    serial.println("Pin " #number " is " func); \
    waitForKey(serial);

#define SET_PIN(number, port, bit)  \
    PORT_DIR(port) |= 1<<((bit)&0x7); \
    PORT_OUT(port) |= 1<<((bit)&0x7); \
    serial.println("Pin " #number); \
    waitForKey(serial); \
    PORT_OUT(port) &= ~(1<<((bit)&0x7));

#if defined(ARDUINO_AVR_MEGA2560)

    void eb_pinTest(HardwareSerial& serial) {
        SET_PIN(1,   ADDR_HIGH, 14);
        SET_PIN(2,   ADDR_HIGH, 12);
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
        SET_PIN(21,  ADDR_HIGH, 10);
        SET_PIN(22,  CONTROL,   5);
        SET_PIN(23,  ADDR_HIGH, 11);
        SET_PIN(24,  ADDR_HIGH, 9);
        SET_PIN(25,  ADDR_HIGH, 8);
        SET_PIN(26,  ADDR_HIGH, 13);
        SET_PIN(27,  CONTROL,   6);
        SKIP_PIN(28,  "+5V");
    }

#elif defined(ARDUINO_AVR_NANO)

    #define SET_ADDR_PIN(number, addr_bit) \
        setAddress(1 << (addr_bit)); \
        serial.println("Pin " #number); \
        waitForKey(serial); \
        setAddress(0);

    #define SET_DATA_PIN(number, data_bit) \
        setDataWriteMode(); \
        writeData(1 << (data_bit)); \
        serial.println("Pin " #number); \
        waitForKey(serial); \
        writeData(0);

    void eb_pinTest(HardwareSerial& serial) {
        SET_ADDR_PIN(1, 14);
        SET_ADDR_PIN(2, 12);
        SET_ADDR_PIN(3,  7);
        SET_ADDR_PIN(4,  6);
        SET_ADDR_PIN(5,  5);
        SET_ADDR_PIN(6,  4);
        SET_ADDR_PIN(7,  3);
        SET_ADDR_PIN(8,  2);
        SET_ADDR_PIN(9,  1);
        SET_ADDR_PIN(10, 0);
        SET_DATA_PIN(11, 0);
        SET_DATA_PIN(12, 1);
        SET_DATA_PIN(13, 2);
        SKIP_PIN(14, "ground");

        SET_DATA_PIN(15, 3);
        SET_DATA_PIN(16, 4);
        SET_DATA_PIN(17, 5);
        SET_DATA_PIN(18, 6);
        SET_DATA_PIN(19, 7);
        SET_ADDR_PIN(20, 15);
        SET_ADDR_PIN(21, 10);
        SET_PIN(22, CONTROL, 1);
        SET_ADDR_PIN(23, 11);
        SET_ADDR_PIN(24,  9);
        SET_ADDR_PIN(25,  8);
        SET_ADDR_PIN(26, 13);
        SET_PIN(27, CONTROL, 0);
        SKIP_PIN(28,  "+5V");
    }
#endif
