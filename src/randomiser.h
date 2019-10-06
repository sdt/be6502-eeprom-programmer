#ifndef INCLUDE_RANDOMISER_H
#define INCLUDE_RANDOMISER_H

class HardwareSerial;

// Use a byte in the eeprom as the random seed. Each time this is called, it
// reads a byte from eeprom, uses it as the seed, increments it, and writes back
// the new value.
extern void randomiser_init(HardwareSerial& serial);

#endif // INCLUDE_RANDOMISER_H
