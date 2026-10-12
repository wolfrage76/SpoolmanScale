#pragma once

#include <lvgl.h>

#include "ui/ams_board_model.h"
#include "ui/theme.h"

// What the files of the AMS View page share, see ams_board_view.cpp for where
// the code comes from and why it is kept in that project's words. The page is
// one file over there. Here it is three, because no file of this firmware is
// longer than a thousand lines:
//
//   ams_board_view.cpp    what the page knows, and how it follows the server
//   ams_board_build.cpp   the widgets, built once
//   ams_board_swatch.cpp  a bay's colour field: stripes, gradient, finishes
//
// Nothing outside those three includes this.

namespace amsboard {


// The page is laid out for this size. The board only opens on a panel that
// has it.
constexpr int SCREEN_WIDTH = 800;
constexpr int SCREEN_HEIGHT = 480;
constexpr int AMS2PRO_COUNT = 4;
constexpr int SPOOL_LOW_PERCENT = 30;
constexpr int SPOOL_CRITICAL_PERCENT = 15;

// The page's palette by the names its code uses, each one a colour of the
// running theme (ui/theme_palette.h). They are variables filled at boot, so
// these are names and not constants.
#define COL_BG           UI_COL_GROUND
#define COL_SURFACE      UI_COL_SURFACE
#define COL_SURFACE_2    UI_COL_ROW
#define COL_BORDER       UI_COL_LINE         // a loaded bay's border
#define COL_BORDER_SOFT  UI_COL_LINE_SOFT
#define COL_TEXT         UI_COL_INK
#define COL_MUTED        UI_COL_INK_SOFT     // colour name, climate, the remaining figure
#define COL_DIM          UI_COL_CAPTION      // the brand, an empty bay's border
#define COL_ACCENT       UI_COL_ACCENT       // the bay that feeds, a full enough spool
#define COL_WARN         UI_COL_WARN         // a spool running low
#define COL_DANGER       UI_COL_BAD          // a spool nearly empty
#define COL_INFO         UI_COL_STATUS_BLUE
#define COL_AMS_CARD     UI_COL_SURFACE      // a unit's card, an empty bay
#define COL_AMS_SOFT     UI_COL_ROW          // a loaded bay
#define COL_AMS_LINE     UI_COL_DIVIDER      // a unit card's border
#define COL_BAR_TRACK    UI_COL_RULE         // the remaining bar behind its fill
// On a swatch, fixed: see ui/theme.h.
#define COL_AMS_INK_DARK   UI_COL_ON_ACCENT_DARK
#define COL_AMS_INK_LIGHT  UI_COL_ON_ACCENT_LIGHT
#define COL_GLOW_FRAME     UI_COL_AMSB_GLOW_FRAME

struct SwatchPaint {
  uint8_t count = 0;
  uint8_t style = 0;  // SwatchStyle
  uint8_t sparkle = 0;  // SwatchSparkle
  uint8_t fiber = 0;    // SwatchFiber
  uint32_t colors[MAX_SLOT_COLORS] = {};
};

struct OverviewBay {
  lv_obj_t *root = nullptr;
  lv_obj_t *swatch = nullptr;
  lv_obj_t *swatchFill = nullptr;
  lv_obj_t *frost = nullptr;
  lv_obj_t *highlight = nullptr;
  lv_obj_t *overlay = nullptr;
  lv_obj_t *bayLabel = nullptr;
  lv_obj_t *material = nullptr;
  lv_obj_t *desc = nullptr;
  lv_obj_t *climate = nullptr;
  lv_obj_t *remainingRow = nullptr;
  lv_obj_t *remainingBar = nullptr;
  lv_obj_t *pct = nullptr;
  lv_obj_t *brand = nullptr;
  SwatchPaint paint;
  uint32_t swatchSig = 0;
};

struct OverviewCard {
  lv_obj_t *root = nullptr;
  lv_obj_t *head = nullptr;
  lv_obj_t *label = nullptr;
  lv_obj_t *climateBox = nullptr;
  lv_obj_t *tempIcon = nullptr;
  lv_obj_t *tempText = nullptr;
  lv_obj_t *humIcon = nullptr;
  lv_obj_t *humText = nullptr;
  lv_obj_t *bayRow = nullptr;
  OverviewBay bays[MAX_OVERVIEW_SLOTS];
  int amsId = 0;
};

// One card per unit, built once and bound to whichever unit it shows.
extern OverviewCard overviewCards[MAX_OVERVIEW_CARDS];
extern lv_obj_t *overviewRoot;
extern lv_obj_t *overviewMain;
extern lv_obj_t *overviewAux;
extern lv_obj_t *overviewHint;
// Who hears a tap on a card.
extern lv_event_cb_t cardTapped;

// ams_board_swatch.cpp
uint32_t swatchInk(uint32_t rgb);
void applyColorSwatch(lv_obj_t *swatch, lv_obj_t **fillOut, lv_obj_t **frostOut,
                      lv_obj_t **highlightOut, lv_obj_t **overlayOut, SwatchPaint *paint,
                      uint32_t &sig, const uint32_t *colors, uint8_t count, SwatchStyle style,
                      SwatchFinish finish, SwatchSparkle sparkle, SwatchFiber fiber,
                      uint32_t fallback, bool empty);

// ams_board_build.cpp
void buildOverviewPage(lv_obj_t *parent);

}  // namespace amsboard
