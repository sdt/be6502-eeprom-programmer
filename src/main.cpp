#include <Arduino.h>
#include "eeprom_burner.h"
#include "srec.h"

static char s_buffer[c_srecBufferSize + 1];
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

    eb_init();

    // This bugged me for ages. Connecting to the serial port on the arduino
    // causes it to reset. The sending script will send whatever, but the
    // arduino would be busy resetting, and miss it, and then they'd all be
    // out of sync.
    //
    // Running `stty -F $PORT -hupl` fixes the problem, but that gets reset
    // by various things.
    //
    // What works is to send RESET here, which causes the script to retry the
    // begin sequence. The script does the stty fix at the same time, ready for
    // the next run.
    Serial.print("RESET\n");
}

void loop() {
    uint16_t bytesRead = readBytesUntil('\n', s_buffer, sizeof(s_buffer), 1000);

    if (bytesRead <= 1) {
        // Ignore this case. No input, or just a newline.
        return;
    }

    if (s_buffer[bytesRead-1] != '\n') {
        // If the final char of the buffer isn't a newline, we either timed out
        // or we reached the end of the buffer.
        if (bytesRead == sizeof(s_buffer)) {
            nak("Buffer overrun");
        }
        else {
            nak("Serial read timeout");
        }
        return;
    }

    // If we get here, we got a complete line.
    s_buffer[bytesRead-1] = 0;
    s_state();
}

static void stateIdle() {
    if (strcmp(s_buffer, "BEGIN") == 0) {
        eb_beginSession();
        s_state = stateActive;
        ack("BEGIN");
        return;
    }

    nak("Unexpected in idle state", s_buffer);
}

static void stateActive() {
    if (strcmp(s_buffer, "BEGIN") == 0) {
        // We might have this happen if the previous write failed midway
        // through, and we never got an END. Rather than reject it, just
        // allow it.
        //
        // Not sure if this still applies. It might not hurt to do it anyway.
        eb_beginSession();
        ack("BEGIN");
        return;
    }

    if (strcmp(s_buffer, "END") == 0) {
        eb_endSession(true);
        s_state = stateIdle;
        ack("END");
        return;
    }

    if (strcmp(s_buffer, "ERASE") == 0) {
        ebError status = eb_chipErase();
        if (status == ebError_OK) {
            ack("ERASE");
        }
        else {
            nak("Chip erase failed", eb_errorMessage(status));
        }
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

// Reads bytes until the terminator is reached, or we hit the end of the buffer,
// or the timeout elapses between bytes being received. Each time a character
// is received, the timeout is extended.
static uint16_t readBytesUntil(char terminator, char *buffer, size_t length, uint32_t timeout) {
    uint32_t startMillis = millis();
    uint16_t index = 0;
    while ((index < length) && (millis() - startMillis < timeout)) {
        int c = Serial.read();
        if (c < 0)
            continue;

        startMillis = millis(); // we got a character - reset the timer

        *buffer++ = (char)c;
        index++;

        if (c == terminator)
            break;
    }
    return index; // return number of characters
}
