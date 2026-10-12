#pragma once

#include <Wire.h>
#include <stdint.h>

// GT911 capacitive touch controller, polled over I2C.
namespace gt911 {

struct Point {
  uint16_t x;
  uint16_t y;
};

// Finds the chip at either of its two addresses and reads the resolution it
// reports in. The caller has released its reset before this. False when
// neither address answers.
bool begin(TwoWire& wire);

// The fingers down, in the chip's own coordinates. The chip has a new frame
// every 10 ms or so; between frames this hands back the last one.
uint8_t read(Point* points, uint8_t max_points);

// The largest coordinates the chip reports, from its configuration.
uint16_t maxX();
uint16_t maxY();

// The level of its INT line while nothing touches the glass, which is what a
// wake-up from deep sleep has to wait for the opposite of.
bool intIdleHigh();

// Takes back a frame that was read and never acknowledged, so INT returns to
// its idle level.
void clearStatus();

}
