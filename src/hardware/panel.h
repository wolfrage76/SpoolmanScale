#pragma once

#include <stdint.h>

// ============================================================
//  WHAT A BOARD'S PANEL HAS TO DO
//
//  hardware/display.cpp is the LVGL side of the display and the same on every
//  board: draw buffer, flush, gamma, screenshots, the touch gesture hook. It
//  talks to the glass through these functions, and each board implements them
//  in boards/<board>/panel.cpp.
//
//  All coordinates are the UI's: DISPLAY_W_PX x DISPLAY_H_PX, origin top
//  left. A panel with more pixels than that enlarges the picture itself and
//  maps the touch back, so nothing above this line knows its real size.
// ============================================================

struct PanelTouchPoint {
  int16_t x;
  int16_t y;
};

// Bus, panel and touch up, the screen black. False when the panel could not
// be started; panelWrite() then has to take its strips and drop them.
bool panelBegin();

// One finished strip, w x h RGB565 pixels in rows, to go to x, y. All four
// are multiples of BOARD_PANEL_ALIGN_X and _Y.
void panelWrite(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* px);

// The fingers on the glass right now, up to max_points of them. Returns how
// many.
uint8_t panelTouchRead(PanelTouchPoint* points, uint8_t max_points);

// Backlight level, 0..255. Only called with BOARD_BACKLIGHT_PWM 1: a board
// whose backlight merely switches is dimmed in the flush instead.
void panelSetBrightness(uint8_t brightness);

// Backlight fully off, and back again at a level.
void panelBacklightOff();
void panelBacklightOn(uint8_t brightness);

// Dark, and a touch arms the wake-up from deep sleep.
void panelPrepareDeepSleep();

// Only on a board with BOARD_NATIVE_W and _H.
//
// On: panelWrite() and panelTouchRead() work in the panel's own pixels, one to
// one, and nothing is enlarged. Off: the UI's coordinates again.
void panelSetNative(bool on);
// The frame as it stands on the glass, BOARD_NATIVE_W x BOARD_NATIVE_H RGB565
// in rows, or nullptr while the panel is not running.
const uint16_t* panelGlass();
