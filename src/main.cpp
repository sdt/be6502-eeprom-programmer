#include <Arduino.h>
#include "eeprom_burner.h"
#include "srec.h"

static char s_buffer[c_srecBufferSize + 1];
static bool s_partialBuffer = false;
static uint16_t readBytesUntil(char terminator, char *buffer, size_t length, uint32_t timeout);

static void stateIdle();
static void stateActive();

typedef void (State)(void);
static State* s_state;

enum PageOp {
    WritePage  = 'W',
    VerifyPage = 'V',
};

static void ack(const char* message);
static void ack(const SRec1* srec, PageOp op);

static void nak(const char* message);
static void nak(const char* message1, const char* message2);

void setup() {
    Serial.begin(115200);
    s_state = stateIdle;

    eb_init(); // TODO: do this after begin
}

void loop() {
    uint16_t bytesRead = readBytesUntil('\n', s_buffer, sizeof(s_buffer), 250);

    if (s_partialBuffer) {
        // The previous line overflowed the buffer, and we discarded it.
        if (bytesRead == 0) {
            nak("Empty next part of buffer overrun");
        }
        else if (s_buffer[bytesRead-1] != '\n') {
            nak("Next part of buffer overrun");
        }
        else {
            nak("Last part of buffer overrun");
            s_partialBuffer = false;
        }
        return;
    }

    if (bytesRead == 0) {
        s_buffer[0] = 0;
        s_state();
        return;
    }

    if (s_buffer[bytesRead-1] != '\n') {
        nak("First part of buffer overrun");
        s_partialBuffer = true;
        return;
    }

    // If we get here, we got a complete line.
    s_buffer[bytesRead-1] = 0;
    s_state();
}

static void stateIdle() {
    if (s_buffer[0] == 0) {
        return;
    }

    if (strcmp(s_buffer, "BEGIN") == 0) {
        // TODO: capture 6502 bus, set up eeprom burner

        s_state = stateActive;
        ack("BEGIN");
        return;
    }

    nak("Unexpected in idle state", s_buffer);
}

static void stateActive() {
    if (s_buffer[0] == 0) {
        // Do we ack this? I don't think so.
        return;
    }

    if (strcmp(s_buffer, "END") == 0) {
        // TODO: deinit eeprom burner, release 6502 bus, reset 6502

        s_state = stateIdle;
        ack("END");
        return;
    }

    PageOp op;
    if (s_buffer[0] == 'W') {
        op = WritePage;
    }
    else if (s_buffer[0] == 'V') {
        op = VerifyPage;
    }
    else {
        nak("Unexpected in active state", s_buffer);
        // TODO - error state?
        return;
    }

    // If we get here, we're either writing or verifying an srec.
    SRec1* s1 = parseSRec1(s_buffer+1, strlen(s_buffer+1));
    if (s1 == NULL) {
        nak("Invalid srecord", s_buffer);
        // TODO - error state?
        return;
    }

    // In both write and verify case, verify the page first.
    if (eb_verifyPage(s1->address, s1->data, s1->dataSize)) {
        // If the page matches already, ack with a Verify
        ack(s1, VerifyPage);
        return;
    }

    if (op == VerifyPage) {
        nak("Verify failed");
        return;
    }

    ebError status = eb_writePage(s1->address, s1->data, s1->dataSize);
    if (status != ebError_OK) {
        // If the write failed, bail.
        nak("Write failed", eb_errorMessage(status));
        return;
    }

    // We used to immediately verify the page, but the calling script does
    // that anyway, so no real need to double-verify it.

    // All good :+1:
    ack(s1, WritePage);
}

static void ack(const char* message) {
    Serial.print("ACK:");
    Serial.print(message);
    Serial.print("\n");
}

static void ack(const SRec1* srec, PageOp op) {
    Serial.print("ACK:");
    Serial.print(char(op));
    Serial.print(":");
    Serial.print(srec->address, HEX);
    Serial.print(":");
    Serial.print(srec->dataSize, DEC);
    Serial.print("\n");
}

static void nak(const char* message) {
    Serial.print("NAK:");
    Serial.print(message);
    Serial.print("\n");
}

static void nak(const char* message1, const char* message2) {
    Serial.print("NAK:");
    Serial.print(message1);
    Serial.print(": ");
    Serial.print(message2);
    Serial.print("\n");
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
