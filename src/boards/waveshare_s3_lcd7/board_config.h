#pragma once

#include <stdint.h>

// Waveshare ESP32-S3-Touch-LCD-7: ESP32-S3 with 16 MB flash and 8 MB octal
// PSRAM, a 7 inch 800x480 panel on a 16 bit RGB bus, GT911 touch, and a
// CH422G I/O expander for everything the RGB bus left no pin for.
//
// The UI is drawn at 480x320 as on every board and enlarged on the way to the
// glass until it fills the panel: 5:3 across, 3:2 down. See panel.cpp.

#define BOARD_ID     "waveshare-s3-lcd7"
#define BOARD_MODEL  "Waveshare-ESP32-S3-Touch-LCD-7"

// Its own image and its own entry in version.json. An image for another
// board would start on this one and find nothing where it expects it.
#define BOARD_OTA_ASSET     "SpoolmanScale-" BOARD_ID ".bin"
#define BOARD_VERSION_NODE  BOARD_ID

// The backlight hangs on the expander and only switches, so the display dims
// the picture instead. The enlargement turns blocks of 3x2 pixels into 5x3,
// so every redrawn area has to begin and end on such a block.
#define BOARD_BACKLIGHT_PWM     0
#define BOARD_PANEL_ALIGN_X     3
#define BOARD_PANEL_ALIGN_Y     2

// The glass itself, for the one screen that is drawn for it: ui/ams_board.
#define BOARD_NATIVE_W  800
#define BOARD_NATIVE_H  480

// The panel's two bounce buffers take 32 kB of internal RAM, which leaves
// 89 kB free on the main screen against the 64 kB that have to stay free:
// the AMS view found room for no bay at all (10.10.2026). There are 7.5 MB of
// octal PSRAM free at the same moment, so list rows may go there.
#define BOARD_LIST_ROWS_IN_PSRAM  1

// The board's own I2C bus and the expander, before the SD card (whose chip
// select is an expander pin) and the display (whose reset and backlight are).
void boardEarlyInit();

namespace hw_pins {

// The sensor bus, PN532 and NAU7802, on the UART2 connector.
//
// Not the board's I2C connector, although it is right there: that bus carries
// the CH422G, and the CH422G has no address of its own. It answers to one
// address per register, and its mode register is 0x24 - the PN532's address.
// Every frame for the reader would start by rewriting the expander's mode,
// and with it the backlight, both resets and the SD card's chip select.
//
// GPIO43 and GPIO44 are UART0, which the board routes either to the USB to
// UART chip or to this connector, by the switch next to it. With the switch
// on UART2 the two pins are free of the chip, and the firmware talks to the
// computer over the USB connector instead. With the switch on UART1 the chip
// drives GPIO44 and the bus does not work.
//
// SDA is the pin the boot ROM prints its messages on. With SCL idle those are
// starts and stops without a clock, which no I2C device reads as data.
constexpr int8_t I2C_EXT_SDA = 43;   // UART2 connector, TXD
constexpr int8_t I2C_EXT_SCL = 44;   // UART2 connector, RXD

// The one GPIO the board brings out by itself, on the sensor connector. No
// unit of this board was ever wired the old way round, so the safe pin and
// the wire are the same pin: see boards/wt32_sc01_plus/board_config.h for
// why there are two. Left unconnected, driving it does nothing.
constexpr int8_t PN532_RESET_WIRE = 6;
constexpr int8_t PN532_RESET_SAFE = 6;

// The library calls pinMode(INPUT) on this pin and never reads it. GPIO16 is
// the input from the RS485 transceiver, an input already.
constexpr int8_t PN532_IRQ_UNUSED = 16;

// The board's own bus: GT911 touch, CH422G expander, the I2C connector.
constexpr int8_t BOARD_SDA = 8;
constexpr int8_t BOARD_SCL = 9;
constexpr int8_t TOUCH_INT = 4;

// The card's chip select is an expander pin, which the SD library cannot
// drive: it wants a GPIO. boardEarlyInit() selects the card for good, it is
// alone on its bus, and the library gets the output to the RS485 transceiver
// to toggle, where nothing listens.
constexpr int8_t SD_CS   = 15;
constexpr int8_t SD_SCK  = 12;
constexpr int8_t SD_MOSI = 11;
constexpr int8_t SD_MISO = 13;

// The RGB bus. Five bits of blue, six of green, five of red, lowest first:
// the order of a 16 bit RGB565 pixel.
constexpr int8_t LCD_PCLK  = 7;
constexpr int8_t LCD_HSYNC = 46;
constexpr int8_t LCD_VSYNC = 3;
constexpr int8_t LCD_DE    = 5;
constexpr int8_t LCD_DATA[16] = {
  14, 38, 18, 17, 10,        // B3..B7
  39, 0, 45, 48, 47, 21,     // G2..G7
  1, 2, 42, 41, 40,          // R3..R7
};

// CH422G outputs, by bit.
constexpr uint8_t EXIO_TOUCH_RST = 1;
constexpr uint8_t EXIO_BACKLIGHT = 2;
constexpr uint8_t EXIO_LCD_RST   = 3;
constexpr uint8_t EXIO_SD_CS     = 4;   // low selects the card
constexpr uint8_t EXIO_USB_SEL   = 5;   // low: GPIO19/20 are USB, high: CAN

}
