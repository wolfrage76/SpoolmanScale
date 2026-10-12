#include "display.h"

#include "app_config.h"
#include "panel.h"
#include "pins.h"

#include <math.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "ui/touch_feedback.h"

// Set to 1 to enable touch coordinate debug output on Serial.
#define TOUCH_DEBUG 0

// The LVGL side of the display, the same on every board. The glass itself,
// its bus, its touch chip and its backlight are the board's: see panel.h.

static void (*touch_activity_callback)() = nullptr;

// The driver LVGL draws through. At file scope because displaySetNative()
// hands it back to LVGL with another resolution.
static lv_disp_drv_t disp_drv;
static bool native_mode = false;

// The draw buffer is as wide as the widest picture it will ever hold.
#ifdef BOARD_NATIVE_W
static constexpr uint32_t DRAW_BUF_W = BOARD_NATIVE_W;
#else
static constexpr uint32_t DRAW_BUF_W = DISPLAY_W_PX;
#endif

static lv_disp_draw_buf_t draw_buf;
// Lines LVGL renders and flushes in one go. It walks the object tree once per
// strip, so the stock 10 lines made a full frame 32 walks: measured while
// scrolling, a frame took 78 ms and over 60 of them were rendering, not the
// bus. 20 lines cut the time per drawn line by 30 %.
//
// The buffer lives in PSRAM, which measured within 4 % of internal RAM at this
// size and leaves the internal heap to WiFi and TLS. It is not larger because
// of the data cache: 32 kB, shared with the fonts read from flash. 19 kB fit,
// and an 80 line buffer (77 kB) rendered a quarter slower per line than this.
static constexpr uint32_t DRAW_BUF_LINES          = 20;
// Only for a device whose PSRAM does not answer, taken from the internal heap
// at boot, when there is plenty of it.
static constexpr uint32_t DRAW_BUF_FALLBACK_LINES = 10;
static char draw_buf_info[16] = "";

const char* displayDrawBufInfo() { return draw_buf_info; }

// RGB565 lookup applied to every pixel on its way to the panel. Two small
// tables (5-bit R/B, 6-bit G) keep the per-pixel cost to two array reads and
// some shifting. They carry the gamma lift and, on a board whose backlight
// cannot be dimmed, the brightness as well: one pass for both.
static uint8_t  ui_lut5[32];
static uint8_t  ui_lut6[64];
static uint16_t ui_gain = 100;      // 100 == identity
static uint8_t  soft_dim = 255;     // 255 == identity, only below with no PWM
static bool     lut_active = false; // false: the transform is skipped

uint16_t displayGetUiGain() { return ui_gain; }

// The darkest the picture is ever drawn, as a share of full. A backlight at
// duty 10 is dim and still readable, because the eye follows it; pixel values
// cut to 4 % are three grey levels. A quarter keeps every colour apart.
static constexpr float SOFT_DIM_FLOOR = 0.25f;

static void rebuildLut() {
  lut_active = (ui_gain > 100) || (soft_dim < 255);
  if (lut_active) {
    const float inv = 100.0f / (float)ui_gain;   // exponent 1/gamma
    const float dim = (soft_dim >= 255) ? 1.0f
        : SOFT_DIM_FLOOR + (1.0f - SOFT_DIM_FLOOR) * ((float)soft_dim / 255.0f);
    for (int i = 0; i < 32; i++)
      ui_lut5[i] = (uint8_t)lroundf(powf((float)i / 31.0f, inv) * 31.0f * dim);
    for (int i = 0; i < 64; i++)
      ui_lut6[i] = (uint8_t)lroundf(powf((float)i / 63.0f, inv) * 63.0f * dim);
  }
  // Only invalidated areas get re-flushed, so force a full repaint. Guarded
  // because this is also reached from loadPrefs(), before lv_init() has run.
  if (lv_disp_get_default()) lv_obj_invalidate(lv_scr_act());
}

void displaySetUiGain(uint16_t gamma_x100) {
  if (gamma_x100 < 100) gamma_x100 = 100;
  if (gamma_x100 > 300) gamma_x100 = 300;
  ui_gain = gamma_x100;
  rebuildLut();
}

// Flush timing since the last displayFlushStatsTake(). Written by lvgl_flush()
// and read by the performance line in the log, both on the loop task.
static DisplayFlushStats flush_stats = {0, 0, 0};

DisplayFlushStats displayFlushStatsTake() {
  const DisplayFlushStats taken = flush_stats;
  flush_stats = {0, 0, 0};
  return taken;
}

// While displayCaptureFrame() runs, every strip also goes to this sink, row
// by row. The panel cannot be read back (no RD line), so the flush is the
// only place a finished frame passes through.
struct CaptureState {
  DisplayRowSink sink;
  void*          ctx;
  int            next_row;   // rows must arrive whole and in order
  bool           ok;
};
static CaptureState* capture = nullptr;

static void captureStrip(const lv_area_t *area, const lv_color_t *color_p, uint32_t w) {
  if (!capture->ok) return;
  if (area->x1 != 0 || (int)w != DISPLAY_W_PX || area->y1 != capture->next_row) {
    capture->ok = false;
    return;
  }
  for (int32_t y = area->y1; y <= area->y2 && capture->ok; y++) {
    const uint16_t* row = (const uint16_t*)&color_p[(y - area->y1) * w];
    capture->ok = capture->sink(row, (int)y, capture->ctx);
  }
  capture->next_row = area->y2 + 1;
}

static void lvgl_flush(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_p) {
  // The gamma pass counts: it is part of what a flush costs.
  const uint32_t flush_start_us = micros();
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  // Before the gamma pass: the picture as the UI drew it, the same colours the
  // simulator shows, not the lift or the dimming this one panel gets.
  if (capture) captureStrip(area, color_p, w);
  if (lut_active) {
    const uint32_t n = w * h;
    for (uint32_t i = 0; i < n; i++) {
      uint16_t c = color_p[i].full;
      color_p[i].full = (uint16_t)((ui_lut5[(c >> 11) & 0x1F] << 11) |
                                   (ui_lut6[(c >>  5) & 0x3F] <<  5) |
                                   (ui_lut5[ c        & 0x1F]));
    }
  }
  panelWrite(area->x1, area->y1, (int32_t)w, (int32_t)h, (const uint16_t*)color_p);
  const uint32_t flush_us = micros() - flush_start_us;
  if (flush_us > flush_stats.max_us) flush_stats.max_us = flush_us;
  flush_stats.sum_us += flush_us;
  flush_stats.count++;
  lv_disp_flush_ready(drv);
}

bool displayCaptureFrame(DisplayRowSink sink, void* ctx) {
  if (!sink || !lv_disp_get_default()) return false;
  CaptureState state = {sink, ctx, 0, true};
  capture = &state;
  // The whole area, so every layer in it is drawn again, lv_layer_top() too,
  // and LVGL merges whatever else was pending into the one full-width area.
  lv_obj_invalidate(lv_scr_act());
  lv_refr_now(NULL);
  capture = nullptr;
  return state.ok && state.next_row == DISPLAY_H_PX;
}

static bool (*touch_gesture_hook)(uint8_t points) = nullptr;
static bool touch_held_by_gesture = false;

void displaySetTouchGestureHook(bool (*hook)(uint8_t points)) {
  touch_gesture_hook = hook;
}

// What the first finger had pressed loses its look, and its release becomes
// PRESS_LOST instead of a click: the way touch_feedback.cpp cancels a press
// that slid off its button. Runs inside the read, where LVGL has set the
// active input device.
static void dropPressForGesture() {
  lv_indev_t* indev = lv_indev_get_act();
  if (!indev) return;
  lv_obj_t* pressed = indev->proc.types.pointer.act_obj;
  if (pressed) lv_obj_clear_state(pressed, LV_STATE_PRESSED);
  lv_indev_wait_release(indev);
}

// Two points are read; LVGL only ever gets the first. The second is what the
// gesture hook counts.
static constexpr uint8_t TOUCH_POINTS_READ = 2;

#if BOARD_PANEL_ALIGN_X > 1 || BOARD_PANEL_ALIGN_Y > 1
// A panel that enlarges the picture works on whole blocks of pixels, so every
// area LVGL redraws is widened to begin and end on a block. LVGL also asks
// here when it cuts an area into strips, which keeps the strips whole too.
static void lvgl_rounder(lv_disp_drv_t *drv, lv_area_t *area) {
  (void)drv;
  // One to one, there are no blocks.
  if (native_mode) return;
  area->x1 = area->x1 / BOARD_PANEL_ALIGN_X * BOARD_PANEL_ALIGN_X;
  area->y1 = area->y1 / BOARD_PANEL_ALIGN_Y * BOARD_PANEL_ALIGN_Y;
  area->x2 = (area->x2 / BOARD_PANEL_ALIGN_X + 1) * BOARD_PANEL_ALIGN_X - 1;
  area->y2 = (area->y2 / BOARD_PANEL_ALIGN_Y + 1) * BOARD_PANEL_ALIGN_Y - 1;
}
static_assert(DISPLAY_W_PX % BOARD_PANEL_ALIGN_X == 0 && DISPLAY_H_PX % BOARD_PANEL_ALIGN_Y == 0,
              "the panel's blocks do not divide the UI");
#endif

static void lvgl_touch(lv_indev_drv_t *drv, lv_indev_data_t *data) {
  PanelTouchPoint tp[TOUCH_POINTS_READ];
  const uint8_t points = panelTouchRead(tp, TOUCH_POINTS_READ);
  const bool gesture = touch_gesture_hook && touch_gesture_hook(points);
  if (gesture && !touch_held_by_gesture) dropPressForGesture();
  touch_held_by_gesture = gesture;
  if (gesture) {
    data->state = LV_INDEV_STATE_REL;
    if (touch_activity_callback) touch_activity_callback();
    return;
  }
  if (points > 0) {
    const int16_t x = tp[0].x, y = tp[0].y;
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
    if (touch_activity_callback) touch_activity_callback();
    #if TOUCH_DEBUG
    static unsigned long last_log = 0;
    if (millis() - last_log > 200) {
      Serial.printf("TOUCH x=%d y=%d\n", x, y);
      last_log = millis();
    }
    #endif
  } else {
    data->state = LV_INDEV_STATE_REL;
  }
}

bool displayHardwareBegin(void (*touch_activity_cb)()) {
  touch_activity_callback = touch_activity_cb;

  // Without a panel the boot still goes on: LVGL draws into its buffer, the
  // flush goes nowhere, and the web interface and the log stay reachable to
  // say what is wrong. Stopping here would leave the UI code without a
  // display to build on.
  if (!panelBegin()) Serial.println("Display: the panel did not start");

  lv_init();
  uint32_t buf_lines = DRAW_BUF_LINES;
  const char* buf_where = "psram";
  lv_color_t* buf = (lv_color_t*)heap_caps_malloc(
      DRAW_BUF_W * buf_lines * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) {
    Serial.println("Display: no PSRAM for the draw buffer, using internal RAM");
    buf_lines = DRAW_BUF_FALLBACK_LINES;
    buf_where = "int";
    buf = (lv_color_t*)heap_caps_malloc(
        DRAW_BUF_W * buf_lines * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  }
  if (!buf) {
    Serial.println("Display: no memory for the draw buffer");
    return false;
  }
  const uint32_t buf_px = DRAW_BUF_W * buf_lines;
  snprintf(draw_buf_info, sizeof(draw_buf_info), "%s/%u", buf_where, (unsigned)buf_lines);
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, buf_px);
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = DISPLAY_W_PX;
  disp_drv.ver_res = DISPLAY_H_PX;
  disp_drv.flush_cb = lvgl_flush;
#if BOARD_PANEL_ALIGN_X > 1 || BOARD_PANEL_ALIGN_Y > 1
  disp_drv.rounder_cb = lvgl_rounder;
#endif
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lvgl_touch;
  indev_drv.feedback_cb = touchFeedback;
  lv_indev_drv_register(&indev_drv);

  return true;
}

void displaySetBrightness(uint8_t brightness) {
#if BOARD_BACKLIGHT_PWM
  panelSetBrightness(brightness);
#else
  // The backlight only switches, so the level goes into the picture.
  if (brightness == soft_dim) return;
  soft_dim = brightness;
  rebuildLut();
#endif
}

void displayBacklightOff() {
  panelBacklightOff();
}

void displayBacklightOn(uint8_t brightness) {
  panelBacklightOn(brightness);
#if !BOARD_BACKLIGHT_PWM
  displaySetBrightness(brightness);
#endif
}

void displayPrepareDeepSleep() {
  panelPrepareDeepSleep();
}

#ifdef BOARD_NATIVE_W
int displayNativeW() { return BOARD_NATIVE_W; }
int displayNativeH() { return BOARD_NATIVE_H; }
bool displayIsNative() { return native_mode; }

bool displaySetNative(bool on) {
  lv_disp_t* disp = lv_disp_get_default();
  if (!disp) return false;
  if (on == native_mode) return true;
  native_mode = on;
  panelSetNative(on);
  disp_drv.hor_res = on ? BOARD_NATIVE_W : DISPLAY_W_PX;
  disp_drv.ver_res = on ? BOARD_NATIVE_H : DISPLAY_H_PX;
  // Resizes the screen object and asks for everything to be drawn again.
  lv_disp_drv_update(disp, &disp_drv);
  return true;
}

const uint16_t* displayGlass() { return panelGlass(); }
#else
int displayNativeW() { return 0; }
int displayNativeH() { return 0; }
bool displayIsNative() { return false; }
bool displaySetNative(bool on) { (void)on; return false; }
const uint16_t* displayGlass() { return nullptr; }
#endif
