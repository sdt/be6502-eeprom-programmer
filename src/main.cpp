#include <Arduino.h>
#include "eeprom_burner.h"
#include "srec.h"

static char s_buffer[c_srecBufferSize + 1];
static bool s_partialBuffer = false;
static uint16_t readBytesUntil(char terminator, char *buffer, size_t length, uint32_t timeout);

void setup() {
    Serial.begin(115200);
    eb_init();
}

void loop() {
    uint16_t bytesRead = readBytesUntil('\n', s_buffer, sizeof(s_buffer), 250);

    if (bytesRead == 0) {
        Serial.print("OK: ready\n");
        s_partialBuffer = false;
    }
    else if (s_buffer[bytesRead-1] == '\n') {
        if (s_partialBuffer) {
            Serial.print("FAIL: [last part of buffer overrun]\n");
            s_partialBuffer = false;
        }
        else {
            s_buffer[bytesRead-1] = 0;
            SRec1* s1 = parseSRec1(s_buffer, bytesRead-1);
            if (s1 == NULL) {
                Serial.print("FAIL: [error parsing srecord]");
            }
            else {
                // Verify the page before writing. It might already have the
                // data we want.
                if (eb_verifyPage(s1->address, s1->data, s1->dataSize)) {
                    Serial.print("OK: [no changes]");
                }
                else if (eb_writePage(s1->address, s1->data, s1->dataSize)) {
                    if (eb_verifyPage(s1->address, s1->data, s1->dataSize)) {
                        Serial.print("OK: [updated ok]");
                    }
                    else {
                        Serial.print("FAIL: [verify failed]");
                    }
                }
                else {
                    Serial.print("FAIL: [write failed]");
                }
                Serial.print(" address=0x");
                Serial.print(s1->address, HEX);
                Serial.print(" size=");
                Serial.print(s1->dataSize, DEC);
                Serial.print('\n');
            }
        }
    }
    else {
        if (s_partialBuffer) {
            Serial.print("FAIL: [next part of buffer overrun]\n");
        }
        else {
            Serial.print("FAIL: [first part of buffer overrun]\n");
        }
        s_partialBuffer = true;
    }
}

static uint16_t readBytesUntil(char terminator, char *buffer, size_t length, uint32_t timeout) {
    uint32_t startMillis = millis();
    uint16_t index = 0;
    while ((index < length) && (millis() - startMillis < timeout)) {
        int c = Serial.read();
        if (c < 0)
            continue;

        *buffer++ = (char)c;
        index++;

        if (c == terminator)
            break;
    }
    return index; // return number of characters, not including null terminator
}

