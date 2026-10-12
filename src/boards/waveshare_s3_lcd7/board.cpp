#include "boards/board.h"

#include <Arduino.h>
#include <Wire.h>

#include "ch422g.h"
#include "hardware/sd_logger.h"

// What the expander holds from the first moment: the panel and the touch chip
// in reset and the backlight off until panelBegin() brings them up in order,
// the SD card selected, and GPIO19/20 on the USB connector rather than on the
// CAN transceiver. All of that is every bit low.
static constexpr uint8_t EXIO_AT_BOOT = 0x00;
static constexpr uint32_t BOARD_I2C_HZ = 400000;

void boardEarlyInit() {
  Wire.begin(hw_pins::BOARD_SDA, hw_pins::BOARD_SCL, BOARD_I2C_HZ);
  if (ch422g::begin(Wire, EXIO_AT_BOOT)) {
    Serial.println("Board: CH422G expander ready");
    logSD("Board: CH422G expander ready");
  } else {
    // Without it there is no backlight, no panel reset and no SD card. The
    // boot goes on: the web interface and the log still say what happened.
    Serial.println("Board: CH422G expander does not answer on the board I2C bus");
    logSD("Board: CH422G expander does not answer on the board I2C bus");
  }
}
