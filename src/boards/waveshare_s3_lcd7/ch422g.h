#pragma once

#include <Wire.h>
#include <stdint.h>

// CH422G I/O expander, the eight push-pull pins EXIO0..7 as outputs.
//
// The chip has no address of its own: each of its registers is an I2C address
// (0x24 mode, 0x38 the outputs), and a write is that address and one byte.
// The outputs cannot be read back, so the last byte written is kept here.
namespace ch422g {

// Writes the outputs first and switches the pins to output after, so no pin
// shows a level nobody asked for. False when the chip does not answer.
bool begin(TwoWire& wire, uint8_t initial_levels);

// One pin, by bit number. False when the write did not go through.
bool write(uint8_t pin, bool level);

}
