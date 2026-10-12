#pragma once

#include <stdint.h>

// WT32-SC01 Plus: ESP32-S3 with 16 MB flash and 2 MB quad PSRAM, a 480x320
// ST7796 on an 8 bit parallel bus, FT6336U touch, a PWM backlight. The board
// SpoolmanScale was designed on, and the one every default belongs to.

#define BOARD_ID     "wt32-sc01-plus"
#define BOARD_MODEL  "WT32-SC01-Plus"

// The names the fleet already downloads, and the top level of version.json.
#define BOARD_OTA_ASSET     "SpoolmanScale.bin"
#define BOARD_VERSION_NODE  ""

#define BOARD_BACKLIGHT_PWM     1
#define BOARD_PANEL_ALIGN_X     1
#define BOARD_PANEL_ALIGN_Y     1
// 2 MB of quad PSRAM: rows built there made scrolling stutter (24.09.2026,
// see hardware/lvgl_mem.cpp), so a list ends where internal RAM does.
#define BOARD_LIST_ROWS_IN_PSRAM  0

// Nothing to bring up before the SD card and the display.
inline void boardEarlyInit() {}

namespace hw_pins {

constexpr int8_t I2C_EXT_SDA = 10;   // PN532 + NAU7802 bus
constexpr int8_t I2C_EXT_SCL = 11;   // PN532 + NAU7802 bus

// The PN532's reset line exists in two places, and which one is used is a
// property of the device rather than of the firmware.
//
// Every SpoolmanScale built before September 2026 has the orange RST wire on
// the header opposite the labelled one, where the module brings out an output
// rather than its RSTPD_N input. Measured on the bench: with the wire there,
// holding any connector GPIO low leaves the reader answering
// GetFirmwareVersion with a healthy 0x32010607 - the reset simply never
// reached the chip, on any unit, since May 2026.
//
// Moving the wire to RSTPDN makes it work; verified on 2026-08-29, where the
// same probe returned SILENT while held and the reader came back after. But
// that is a soldering job, most of the fleet will never do it, and driving
// PN532_RESET_WIRE on a device that still has the old wiring puts the ESP32's
// output against the module's. So the pin is only handed to the library after
// a measurement on this very device says the wire was moved - see
// services/nfc_reset.h.
constexpr int8_t PN532_RESET_WIRE = 14;   // EXT_IO5, connector pin 7, brown
constexpr int8_t PN532_RESET_SAFE = 12;   // EXT_IO3, pin 5, no build ever wired it

// The PN532's IRQ line is not wired, and in I2C mode the library never reads
// it: isready() answers from the bus, and the branch that would call
// digitalRead(_irq) sits behind that check and is unreachable. Its constructor
// still calls pinMode() on whatever pin number it is handed though, and -1
// arrives there as 255, which logs "Invalid pin selected" on every single boot.
// That line reads like a fault and is not one, so the constructor gets a pin
// that exists and costs nothing: EXT_IO4 on the extension connector, unused by
// this firmware and an input after reset anyway.
constexpr int8_t PN532_IRQ_UNUSED = 13;

constexpr int8_t TOUCH_SDA = 6;      // FT6336U internal board bus
constexpr int8_t TOUCH_SCL = 5;      // FT6336U internal board bus
constexpr int8_t TOUCH_INT = 7;      // FT6336U INT for wake-up

constexpr int8_t LCD_BACKLIGHT = 45;

constexpr int8_t SD_CS = 41;
constexpr int8_t SD_SCK = 39;
constexpr int8_t SD_MOSI = 40;
constexpr int8_t SD_MISO = 38;

}
