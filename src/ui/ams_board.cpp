#include "ams_board.h"

#include <Arduino.h>
#include <lvgl.h>
#include <string.h>

#include "app/app_state.h"
#include "hardware/display.h"
#include "hardware/sd_logger.h"
#include "services/ams_board_feed.h"
#include "services/ams_slots.h"
#include "services/backend_api.h"
#include "services/prefs_store.h"
#include "services/server_reach.h"
#include "ui/ams_board_view.h"
#include "ui/ams_view.h"
#include "ui/navigation.h"
#include "ui/theme.h"
#include "ui/ui_common.h"

#include "lang.h"

// The page's frame is the CrowPanel's: four pixels of air around everything
// and a header of 36 over the page.
#define AMSB_PAD          4
#define AMSB_HDR_H        36
#define AMSB_HDR_GAP      10
#define AMSB_TITLE_W      140
#define AMSB_TOUCH_PAD    12

// How often the page asks the server again while it stands, the CrowPanel's
// ten seconds, and how long a request may hold the loop. Shorter than the AMS
// view's eight seconds: there somebody pressed a button and waits, here
// nobody asked.
#define AMSB_REFRESH_MS   10000
#define AMSB_TIMEOUT_MS   4000
// A bay shows the percentage and the grams in turn, each for this long.
#define AMSB_TOGGLE_MS    10000

// The printer chosen last, shared with the AMS view: both show the same one.
#define AMSB_NVS_PRINTER  "ams_printer"

static lv_obj_t* s_scr    = nullptr;
static lv_obj_t* s_status = nullptr;

static AmsPrinterList s_printers;
static uint8_t        s_printer_idx = 0;
static int            s_printer_id  = 0;

static bool          s_open_pending    = false;
static bool          s_close_pending   = false;
static bool          s_classic_pending = false;
// A bay was tapped: its card, which is the AMS view's and 480x320.
static bool          s_bay_pending     = false;
static int           s_bay_ams         = -1;
static int           s_bay_tray        = -1;
static bool          s_next_pending    = false;
static bool          s_fetch_now       = false;
static unsigned long s_last_fetch_ms   = 0;
static unsigned long s_last_toggle_ms  = 0;

// On a panel with the pixels for it, and with the one backend that describes
// a spool the way the page draws it.
bool amsBoardAvailable() { return displayNativeW() > 0 && amsboard::feedAvailable(); }
bool isAmsBoardOpen()    { return s_scr != nullptr; }

void requestAmsBoard() {
  if (!amsBoardAvailable()) return;
  s_open_pending = true;
}

void destroyAmsBoard() {
  s_open_pending    = false;
  s_close_pending   = false;
  s_classic_pending = false;
  s_bay_pending     = false;
  s_next_pending    = false;
  s_fetch_now       = false;
  if (!s_scr) return;
  // Hidden before the display changes size, so that not one frame shows a
  // corner of this page enlarged.
  lv_obj_add_flag(s_scr, LV_OBJ_FLAG_HIDDEN);
  lv_obj_del(s_scr);
  s_scr = s_status = nullptr;
  amsboard::viewForget();
  displaySetNative(false);
  logSD("AMSBOARD: closed");
}

// ---- drawing ---------------------------------------------------------------

static void setStatus(const char* text) {
  if (s_status) lv_label_set_text(s_status, text);
}

// A symbol in the header that is a button: the CrowPanel's are labels with a
// grown touch area, not buttons with a face.
static lv_obj_t* headerSymbol(lv_obj_t* header, const char* symbol, lv_event_cb_t cb) {
  lv_obj_t* l = lv_label_create(header);
  if (!l) return nullptr;
  lv_obj_set_style_text_font(l, &lv_font_montserrat_ext_18, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(amsboard::viewAccentRgb()), 0);
  lv_label_set_text(l, symbol);
  lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(l, AMSB_TOUCH_PAD);
  lv_obj_add_event_cb(l, cb, LV_EVENT_CLICKED, nullptr);
  return l;
}

static void toClassicCb(lv_event_t* e) {
  (void)e;
  logSD("UI: AMS board -> AMS view");
  s_classic_pending = true;
}

// A tap on a unit. On a bay with a spool in it, that bay's card; anywhere
// else on the unit, the AMS view as the list symbol opens it.
static void cardCb(lv_event_t* e) {
  int ams = -1, tray = -1;
  if (!amsboard::viewBayAt(e, ams, tray)) { toClassicCb(e); return; }
  logSDf("UI: AMS board -> card of bay %d/%d", ams, tray);
  s_bay_ams     = ams;
  s_bay_tray    = tray;
  s_bay_pending = true;
}

static void buildScreen() {
  const lv_coord_t w = (lv_coord_t)displayNativeW();
  const lv_coord_t h = (lv_coord_t)displayNativeH();

  s_scr = lv_obj_create(lv_scr_act());
  if (!s_scr) return;
  lv_obj_remove_style_all(s_scr);
  lv_obj_set_pos(s_scr, 0, 0);
  lv_obj_set_size(s_scr, w, h);
  lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(s_scr, lv_color_hex(amsboard::viewBackgroundRgb()), 0);
  lv_obj_set_style_pad_all(s_scr, AMSB_PAD, 0);
  lv_obj_set_style_pad_row(s_scr, AMSB_PAD, 0);
  lv_obj_set_flex_flow(s_scr, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
  // Takes every touch, so nothing of the 480x320 UI underneath can be hit.
  lv_obj_add_flag(s_scr, LV_OBJ_FLAG_CLICKABLE);

  // The CrowPanel's header row: a way out, the page's name, the printer.
  lv_obj_t* header = lv_obj_create(s_scr);
  if (!header) return;
  lv_obj_remove_style_all(header);
  lv_obj_set_width(header, lv_pct(100));
  lv_obj_set_height(header, AMSB_HDR_H);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(header, AMSB_HDR_GAP, 0);
  lv_obj_set_style_pad_hor(header, AMSB_PAD, 0);

  headerSymbol(header, LV_SYMBOL_LEFT, [](lv_event_t* e) {
    (void)e;
    logSD("BTN: AMS board close");
    s_close_pending = true;
  });

  lv_obj_t* title = lv_label_create(header);
  if (title) {
    lv_obj_set_style_text_font(title, &lv_font_montserrat_ext_22, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(amsboard::viewTextRgb()), 0);
    lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);
    lv_obj_set_width(title, AMSB_TITLE_W);
    lv_label_set_text(title, T(STR_AMSB_TITLE));
  }

  // The printer and what it does. With several printers a tap steps on to
  // the next one.
  s_status = lv_label_create(header);
  if (s_status) {
    lv_obj_set_style_text_font(s_status, &lv_font_montserrat_ext_12, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(amsboard::viewAccentRgb()), 0);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(s_status, 1);
    lv_obj_add_flag(s_status, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_status, AMSB_TOUCH_PAD);
    lv_obj_add_event_cb(s_status, [](lv_event_t* e) {
      (void)e;
      s_next_pending = true;
    }, LV_EVENT_CLICKED, nullptr);
    lv_label_set_text(s_status, "");
  }

  // The AMS view, where a bay's card and the assignments are.
  headerSymbol(header, LV_SYMBOL_LIST, toClassicCb);

  lv_obj_t* content = lv_obj_create(s_scr);
  if (!content) return;
  lv_obj_remove_style_all(content);
  lv_obj_set_width(content, lv_pct(100));
  lv_obj_set_flex_grow(content, 1);
  lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
  lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

  if (!amsboard::viewBuild(content, cardCb)) {
    logSD("AMSBOARD: no memory for the page");
    return;
  }
  amsboard::viewSetHint(T(STR_AMSV_LOADING));
}

// ---- the server -------------------------------------------------------------

static void statusHttp(int code) {
  char fmt[40], line[48];
  copyT(fmt, sizeof(fmt), STR_AMSV_ERR_HTTP);
  snprintf(line, sizeof(line), fmt, code);
  amsboard::viewSetHint(line);
  setStatus(line);
}

// `asked`: the user is looking at a page that just opened, or pressed
// something. The refresh behind a standing page is not, and a server that is
// away for a while must not put up its popup every ten seconds.
static void fetchAndDraw(bool asked) {
  if (!s_scr) return;
  if (!wifi_ok) {
    amsboard::viewSetHint(T(STR_AMSV_ERR_NET));
    setStatus(T(STR_AMSV_ERR_NET));
    return;
  }

  if (s_printers.count == 0) {
    int code = backendListPrinters(s_printers, AMSB_TIMEOUT_MS);
    if (asked) serverReachNote(code, true);
    if (code != 200) { statusHttp(code); return; }
    if (s_printers.count == 0) {
      amsboard::viewSetHint(T(STR_AMSV_NO_PRINTER));
      return;
    }
    // The one chosen last time, here or in the AMS view, if it is still there.
    const int stored = prefsGetInt(AMSB_NVS_PRINTER, 0);
    s_printer_idx = 0;
    for (uint8_t i = 0; i < s_printers.count; i++) {
      if (s_printers.p[i].id == stored) { s_printer_idx = i; break; }
    }
    s_printer_id = s_printers.p[s_printer_idx].id;
  }

  const amsboard::OverviewUnit* units = nullptr;
  int count = 0;
  amsboard::PrinterLine line;
  int code = amsboard::feedFetch(s_printer_id, &units, &count, &line, AMSB_TIMEOUT_MS);
  if (asked) serverReachNote(code, true);
  if (code != 200) {
    // What stands on the page stays: an old board with a note in the header
    // says more than an empty one.
    char fmt[40], msg[48];
    copyT(fmt, sizeof(fmt), STR_AMSV_ERR_HTTP);
    snprintf(msg, sizeof(msg), fmt, code);
    setStatus(msg);
    return;
  }

  // The CrowPanel's summary: name, state, and the job's percentage. With
  // several printers, which of them this is leads, as in the AMS view.
  String summary;
  if (s_printers.count > 1) {
    char fmt[12], of[12];
    copyT(fmt, sizeof(fmt), STR_AMSV_PRN_OF);
    snprintf(of, sizeof(of), fmt, (int)s_printer_idx + 1, (int)s_printers.count);
    summary += of;
  }
  if (line.name.length()) {
    if (summary.length()) summary += "  ";
    summary += line.name;
  }
  if (line.state.length()) {
    if (summary.length()) summary += "  ";
    summary += line.state;
  }
  if (line.progress >= 0) {
    if (summary.length()) summary += "  ";
    summary += String(line.progress);
    summary += "%";
  }
  setStatus(summary.c_str());

  if (count == 0) amsboard::viewSetHint(T(STR_AMSV_NO_AMS));
  amsboard::viewSetUnits(units, count);
}

// ---- the loop -----------------------------------------------------------------

// Whether anything stands above this page: a popup, a note, another screen.
// All of them are laid out for 480x320, so none can be shown while the
// display is at the panel's resolution.
static bool somethingAbove() {
  lv_obj_t* scr = lv_scr_act();
  const uint32_t n = lv_obj_get_child_cnt(scr);
  bool past = false;
  for (uint32_t i = 0; i < n; i++) {
    lv_obj_t* child = lv_obj_get_child(scr, (int32_t)i);
    if (child == s_scr) { past = true; continue; }
    if (past && !lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) return true;
  }
  lv_obj_t* top = lv_layer_top();
  const uint32_t top_n = lv_obj_get_child_cnt(top);
  for (uint32_t i = 0; i < top_n; i++) {
    if (!lv_obj_has_flag(lv_obj_get_child(top, (int32_t)i), LV_OBJ_FLAG_HIDDEN)) return true;
  }
  return false;
}

void handleAmsBoardDeferredActions() {
  if (s_open_pending) {
    s_open_pending = false;
    logSD("UI: Screen -> AMS board");
    // Everything else away first, this page's predecessor included.
    hideAllOverlays();
    if (!displaySetNative(true)) return;
    s_printers.count = 0;
    s_printer_id = 0;
    buildScreen();
    if (!s_scr) { displaySetNative(false); return; }
    s_fetch_now = true;
    return;      // one pass to draw, the next one to fetch
  }

  if (!s_scr) return;

  if (lv_obj_has_flag(s_scr, LV_OBJ_FLAG_HIDDEN) || somethingAbove()) {
    logSD("AMSBOARD: something else came up, stepping aside");
    destroyAmsBoard();
    return;
  }

  if (s_close_pending) {
    showMainScreen();      // takes this page down on its way, see navigation.cpp
    return;
  }

  if (s_bay_pending) {
    const int ams = s_bay_ams, tray = s_bay_tray;
    destroyAmsBoard();
    requestAmsViewBay(ams, tray);
    return;
  }

  if (s_classic_pending) {
    destroyAmsBoard();
    requestAmsView(AMS_VIEW_BROWSE);
    return;
  }

  if (s_next_pending) {
    s_next_pending = false;
    if (s_printers.count > 1) {
      s_printer_idx = (uint8_t)((s_printer_idx + 1) % s_printers.count);
      s_printer_id  = s_printers.p[s_printer_idx].id;
      prefsPutInt(AMSB_NVS_PRINTER, s_printer_id);
      logSDf("AMSBOARD: switched to printer %d \"%s\"", s_printer_id,
             s_printers.p[s_printer_idx].name);
      setStatus(T(STR_AMSV_LOADING));
      s_fetch_now = true;
      return;    // one pass to say so, the next one to fetch
    }
  }

  const bool due = (millis() - s_last_fetch_ms) >= AMSB_REFRESH_MS;
  if (s_fetch_now || due) {
    const bool asked = s_fetch_now;
    s_fetch_now = false;
    fetchAndDraw(asked);
    s_last_fetch_ms = millis();
  }

  if (millis() - s_last_toggle_ms >= AMSB_TOGGLE_MS) {
    s_last_toggle_ms = millis();
    amsboard::viewTogglePercentAndGrams();
  }
}
