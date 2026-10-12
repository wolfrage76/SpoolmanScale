#pragma once

// ============================================================
//  THE BOARD THIS FIRMWARE IS BUILT FOR
//
//  One firmware, several boards. Everything a board decides lives in its own
//  folder under boards/, and nothing outside that folder names a board:
//
//    board_config.h   its identity, its pins and what its hardware can do
//    panel.cpp        display, touch and backlight behind hardware/panel.h
//    board.cpp        boardEarlyInit(), when the board needs one
//
//  The environment in platformio.ini picks the board twice: it defines one
//  BOARD_* macro, which selects the header below, and its build_src_filter
//  compiles that board's folder and no other.
//
//  What a board_config.h has to provide:
//
//    BOARD_ID                the environment's name, as in platformio.ini
//    BOARD_MODEL             the name a person would know it by
//    BOARD_OTA_ASSET         the image's file name in a GitHub release
//    BOARD_VERSION_NODE      this board's object in version.json, or "" for
//                            the top level
//    BOARD_BACKLIGHT_PWM     1 when the backlight can be dimmed, 0 when it
//                            only switches: the display then dims the picture
//    BOARD_PANEL_ALIGN_X     1, or the width in pixels of the blocks a panel
//    BOARD_PANEL_ALIGN_Y     that enlarges the picture works on, and their
//                            height: every redrawn area then begins and ends
//                            on a block
//    BOARD_LIST_ROWS_IN_PSRAM  1 when list rows may be built in PSRAM once
//                            internal RAM is short. Octal PSRAM only: out of
//                            quad PSRAM a list stutters as it scrolls
//    BOARD_NATIVE_W          only on a board whose panel has more pixels than
//    BOARD_NATIVE_H          the UI: the panel's real size. A screen written
//                            for it can then have every one of them, see
//                            displaySetNative()
//    namespace hw_pins       the pins the shared code asks for
//    boardEarlyInit()        runs first in setup(), before the SD card and
//                            the display
//
//  A new board is a new folder, one more branch below and one more
//  environment. See BUILDING.md, "Adding a board".
// ============================================================

#if defined(BOARD_WT32_SC01_PLUS)
  #include "boards/wt32_sc01_plus/board_config.h"
#elif defined(BOARD_WAVESHARE_S3_LCD7)
  #include "boards/waveshare_s3_lcd7/board_config.h"
#else
  #error "No board selected: the environment in platformio.ini must define a BOARD_* macro"
#endif
