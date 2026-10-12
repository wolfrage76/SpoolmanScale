#include "hardware/panel.h"

#include "boards/board.h"
#include "hardware/display.h"
#include "hardware/sd_logger.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_lcd_panel_ops.h>
#include <esp_heap_caps.h>
#include <esp_lcd_panel_rgb.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <string.h>

#include "ch422g.h"
#include "gt911.h"

// ============================================================
//  800x480 OF GLASS FOR A 480x320 PICTURE
//
//  The panel has no memory of its own: the ESP32 clocks every pixel out of a
//  frame buffer in PSRAM, some thirty times a second. panelWrite() enlarges
//  each strip LVGL hands over into that buffer until the picture fills the
//  glass: three pixels become five across, two lines become three down.
//
//  That is 5:3 one way and 3:2 the other, so everything is drawn 11 % wider
//  than it was designed. The alternative, 3:2 both ways, keeps circles round
//  and leaves 40 px of black on either side of a 7 inch screen, which looked
//  like a fault (10.10.2026).
// ============================================================

static constexpr int PANEL_W = 800;
static constexpr int PANEL_H = 480;
// A block of the picture and what it becomes on the glass.
static constexpr int IN_W = BOARD_PANEL_ALIGN_X, OUT_W = 5;
static constexpr int IN_H = BOARD_PANEL_ALIGN_Y, OUT_H = 3;
static_assert(IN_W == 3 && IN_H == 2, "panelWrite() is written for blocks of 3x2");
static_assert(DISPLAY_W_PX * OUT_W / IN_W == PANEL_W && DISPLAY_H_PX * OUT_H / IN_H == PANEL_H,
              "the enlarged picture is not the size of the panel");

// The panel's timing. The porches are its maker's. The pixel clock is not: at
// the maker's 16 MHz the frame buffer has to be read out of PSRAM at 31 MB a
// second, and this board reads 30 (measured 10.10.2026, 525 us for a slice
// that has 512). The picture rolled and left pieces of itself behind. 14 MHz
// over 820 x 500 clocks is 34 frames a second, which a scale does not miss.
static constexpr uint32_t LCD_PCLK_HZ     = 14000000;
static constexpr uint32_t LCD_HSYNC_PULSE = 4;
static constexpr uint32_t LCD_HSYNC_BACK  = 8;
static constexpr uint32_t LCD_HSYNC_FRONT = 8;
static constexpr uint32_t LCD_VSYNC_PULSE = 4;
static constexpr uint32_t LCD_VSYNC_BACK  = 8;
static constexpr uint32_t LCD_VSYNC_FRONT = 8;

// The frame buffer is in PSRAM, and PSRAM shares its bus with the flash. Read
// from there directly, every flash write (a setting, a log line, an update)
// and every burst of WiFi stalls the pixel clock and the picture jumps
// sideways. With bounce buffers the driver copies the frame through two small
// buffers in internal RAM, ten lines at a time, and the clock never waits.
// A flash write still holds the copy up for a moment; the driver notices the
// missing turn and starts the next frame in step again, which needs
// CONFIG_LCD_RGB_RESTART_IN_VSYNC off, see platformio.ini.
//
// The copy runs in an interrupt, and an interrupt belongs to the core that
// asked for it. That is core 1, the loop task's, because panelBegin() is
// called from setup(): on core 0 it would wait behind WiFi and Bluetooth,
// which keep interrupts off for longer than a slice lasts.
static constexpr size_t LCD_BOUNCE_LINES = 10;
static constexpr size_t LCD_DMA_BURST_BYTES = 64;

static esp_lcd_panel_handle_t s_panel = nullptr;
static uint16_t* s_fb = nullptr;
static bool s_touch_ok = false;
// One to one instead of enlarged, see panelSetNative().
static bool s_native = false;
static_assert(BOARD_NATIVE_W == PANEL_W && BOARD_NATIVE_H == PANEL_H,
              "BOARD_NATIVE_W and _H are not the panel's size");

// A restart resets the processor and not the peripherals: the panel's DMA
// would go on reading the frame out of PSRAM while the bootloader loads the
// firmware over the same bus. That boot stalled every time, until the
// hardware watchdog reset the chip a few seconds later and the log called a
// plain restart a watchdog reset (10.10.2026). So the panel is stopped first,
// dark, whoever asks for the restart.
static void panelStop() {
  ch422g::write(hw_pins::EXIO_BACKLIGHT, false);
  if (!s_panel) return;
  s_fb = nullptr;
  esp_lcd_panel_del(s_panel);
  s_panel = nullptr;
}

static bool rgbBegin() {
  esp_lcd_rgb_panel_config_t cfg = {};
  cfg.clk_src = LCD_CLK_SRC_DEFAULT;
  cfg.timings.pclk_hz = LCD_PCLK_HZ;
  cfg.timings.h_res = PANEL_W;
  cfg.timings.v_res = PANEL_H;
  cfg.timings.hsync_pulse_width = LCD_HSYNC_PULSE;
  cfg.timings.hsync_back_porch  = LCD_HSYNC_BACK;
  cfg.timings.hsync_front_porch = LCD_HSYNC_FRONT;
  cfg.timings.vsync_pulse_width = LCD_VSYNC_PULSE;
  cfg.timings.vsync_back_porch  = LCD_VSYNC_BACK;
  cfg.timings.vsync_front_porch = LCD_VSYNC_FRONT;
  cfg.timings.flags.pclk_active_neg = 1;
  cfg.data_width = 16;
  cfg.bits_per_pixel = 16;
  cfg.num_fbs = 1;
  cfg.bounce_buffer_size_px = PANEL_W * LCD_BOUNCE_LINES;
  cfg.dma_burst_size = LCD_DMA_BURST_BYTES;
  cfg.hsync_gpio_num = hw_pins::LCD_HSYNC;
  cfg.vsync_gpio_num = hw_pins::LCD_VSYNC;
  cfg.de_gpio_num    = hw_pins::LCD_DE;
  cfg.pclk_gpio_num  = hw_pins::LCD_PCLK;
  cfg.disp_gpio_num  = -1;
  for (int i = 0; i < 16; i++) cfg.data_gpio_nums[i] = hw_pins::LCD_DATA[i];
  cfg.flags.fb_in_psram = 1;

  esp_err_t err = esp_lcd_new_rgb_panel(&cfg, &s_panel);
  if (err == ESP_OK) err = esp_lcd_panel_reset(s_panel);
  if (err == ESP_OK) err = esp_lcd_panel_init(s_panel);
  if (err != ESP_OK) {
    Serial.printf("Panel: the RGB driver refused, error 0x%x\n", (unsigned)err);
    return false;
  }
  void* fb = nullptr;
  if (esp_lcd_rgb_panel_get_frame_buffer(s_panel, 1, &fb) != ESP_OK || !fb) {
    Serial.println("Panel: no frame buffer");
    return false;
  }
  s_fb = (uint16_t*)fb;
  memset(s_fb, 0, (size_t)PANEL_W * PANEL_H * sizeof(uint16_t));
  esp_register_shutdown_handler(panelStop);
  return true;
}

// The GT911 takes its I2C address from the level of its INT line at the end
// of reset: low gives 0x5D. The line is the chip's output afterwards.
static void touchBegin() {
  static constexpr uint32_t RESET_HOLD_MS = 10;
  static constexpr uint32_t ADDR_HOLD_MS  = 10;
  static constexpr uint32_t BOOT_MS       = 60;
  pinMode(hw_pins::TOUCH_INT, OUTPUT);
  digitalWrite(hw_pins::TOUCH_INT, LOW);
  ch422g::write(hw_pins::EXIO_TOUCH_RST, false);
  delay(RESET_HOLD_MS);
  ch422g::write(hw_pins::EXIO_TOUCH_RST, true);
  delay(ADDR_HOLD_MS);
  pinMode(hw_pins::TOUCH_INT, INPUT);
  delay(BOOT_MS);
  s_touch_ok = gt911::begin(Wire);
  if (s_touch_ok) {
    Serial.printf("Touch: GT911 ready, reports %u x %u\n",
                  (unsigned)gt911::maxX(), (unsigned)gt911::maxY());
    // In the log as well: it is the one place the glass says how large it is,
    // and a panel of another size shows up here before anywhere else.
    logSDf("Touch: GT911 ready, reports %u x %u (panel driven at %d x %d)",
           (unsigned)gt911::maxX(), (unsigned)gt911::maxY(), PANEL_W, PANEL_H);
  } else {
    Serial.println("Touch: GT911 does not answer");
    logSD("Touch: GT911 does not answer");
  }
}

bool panelBegin() {
  static constexpr uint32_t LCD_RESET_SETTLE_MS = 20;
  // boardEarlyInit() has held the panel in reset since power-up.
  ch422g::write(hw_pins::EXIO_LCD_RST, true);
  delay(LCD_RESET_SETTLE_MS);
  const bool lcd_ok = rgbBegin();
  if (!lcd_ok) logSD("Panel: the RGB driver did not start, see the serial log");
  // What the panel costs in internal RAM is its two bounce buffers. The
  // figure after them is what the rest of the firmware starts with.
  logSDf("Panel: %d x %d at %u MHz, %u bounce lines, internal heap %u free",
         PANEL_W, PANEL_H, (unsigned)(LCD_PCLK_HZ / 1000000), (unsigned)LCD_BOUNCE_LINES,
         (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  touchBegin();
  // Only now, over a frame buffer that is black: no flash of whatever the
  // PSRAM held.
  if (lcd_ok) ch422g::write(hw_pins::EXIO_BACKLIGHT, true);
  return lcd_ok;
}

// The mean of two RGB565 pixels, each channel by itself: the bits two pixels
// share, plus half of those they differ in, with the lowest bit of every
// channel masked so that the shift does not carry it into its neighbour.
static inline uint16_t mix(uint16_t a, uint16_t b) {
  return (uint16_t)((a & b) + (((a ^ b) & 0xF7DE) >> 1));
}

// Every block of 3x2 pixels in the strip becomes 5x3 on the glass. Across,
// the three pixels keep their places at either end and in the middle, and the
// two between are the mean of their neighbours. Down, the same: both lines as
// they are and their mean between them. x and w are multiples of 3, y and h of
// 2 (BOARD_PANEL_ALIGN_X, _Y), so a strip is whole blocks and no block needs a
// pixel from the strip before or after.
void panelWrite(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* px) {
  if (!s_fb) return;
  if (s_native) {
    for (int32_t row = 0; row < h; row++) {
      memcpy(s_fb + (y + row) * PANEL_W + x, px + row * w, (size_t)w * sizeof(uint16_t));
    }
    return;
  }
  const int32_t out_x = x / IN_W * OUT_W;
  const int32_t out_y = y / IN_H * OUT_H;
  for (int32_t row = 0; row + IN_H <= h; row += IN_H) {
    const uint16_t* s0 = px + row * w;
    const uint16_t* s1 = s0 + w;
    uint16_t* d0 = s_fb + (out_y + row / IN_H * OUT_H) * PANEL_W + out_x;
    uint16_t* d1 = d0 + PANEL_W;
    uint16_t* d2 = d1 + PANEL_W;
    for (int32_t col = 0; col + IN_W <= w; col += IN_W) {
      const uint16_t a = s0[col], b = s0[col + 1], c = s0[col + 2];
      const uint16_t d = s1[col], e = s1[col + 1], f = s1[col + 2];
      const uint16_t ab = mix(a, b), bc = mix(b, c);
      const uint16_t de = mix(d, e), ef = mix(e, f);
      d0[0] = a; d0[1] = ab; d0[2] = b; d0[3] = bc; d0[4] = c;
      d2[0] = d; d2[1] = de; d2[2] = e; d2[3] = ef; d2[4] = f;
      d1[0] = mix(a, d);  d1[1] = mix(ab, de); d1[2] = mix(b, e);
      d1[3] = mix(bc, ef); d1[4] = mix(c, f);
      d0 += OUT_W; d1 += OUT_W; d2 += OUT_W;
    }
  }
}

static int16_t clampTo(int32_t v, int32_t max) {
  if (v < 0) return 0;
  if (v > max) return (int16_t)max;
  return (int16_t)v;
}

// From the chip's coordinates straight to the UI's: the picture covers the
// whole glass, so the two only differ in scale.
uint8_t panelTouchRead(PanelTouchPoint* points, uint8_t max_points) {
  static constexpr uint8_t READ_MAX = 5;
  if (!s_touch_ok) return 0;
  gt911::Point raw[READ_MAX];
  if (max_points > READ_MAX) max_points = READ_MAX;
  const uint8_t n = gt911::read(raw, max_points);
  const int32_t chip_w = gt911::maxX() ? gt911::maxX() : PANEL_W;
  const int32_t chip_h = gt911::maxY() ? gt911::maxY() : PANEL_H;
  const int32_t out_w = s_native ? PANEL_W : DISPLAY_W_PX;
  const int32_t out_h = s_native ? PANEL_H : DISPLAY_H_PX;
  for (uint8_t i = 0; i < n; i++) {
    points[i].x = clampTo((int32_t)raw[i].x * out_w / chip_w, out_w - 1);
    points[i].y = clampTo((int32_t)raw[i].y * out_h / chip_h, out_h - 1);
  }
  return n;
}

void panelSetNative(bool on) { s_native = on; }

const uint16_t* panelGlass() { return s_fb; }

// Never called: BOARD_BACKLIGHT_PWM is 0 and the display dims the picture.
void panelSetBrightness(uint8_t brightness) {
  (void)brightness;
}

void panelBacklightOff() {
  ch422g::write(hw_pins::EXIO_BACKLIGHT, false);
}

void panelBacklightOn(uint8_t brightness) {
  (void)brightness;
  ch422g::write(hw_pins::EXIO_BACKLIGHT, true);
}

// The expander stays powered in deep sleep and keeps its outputs, so the
// backlight has to be switched off here or it would light a dead panel. The
// touch chip stays awake as well and moves its INT line on a touch: away from
// the level it rests at, which depends on how the chip was configured.
void panelPrepareDeepSleep() {
  ch422g::write(hw_pins::EXIO_BACKLIGHT, false);
  if (!s_touch_ok) return;
  gt911::clearStatus();
  esp_sleep_enable_ext0_wakeup((gpio_num_t)hw_pins::TOUCH_INT,
                               gt911::intIdleHigh() ? 0 : 1);
}
