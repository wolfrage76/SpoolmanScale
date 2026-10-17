#include "hardware/panel.h"

#include "app_config.h"
#include "boards/board.h"

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

// LovyanGFX talks to the FT6336 on I2C port 0. Register 0xA4 is the interrupt
// mode: 0 is polling, which does not hold INT low for a touch, and 1 is
// trigger mode, which does until the points are read.
static constexpr int      FT6336_I2C_PORT       = 0;
static constexpr uint8_t  FT6336_ADDR           = 0x38;
static constexpr uint8_t  FT6336_INTMODE_REG    = 0xA4;
static constexpr uint8_t  FT6336_INTMODE_TRIGGER = 0x01;

// No TwoWire object for the touch bus. LovyanGFX owns it: the Touch_FT5x06
// config below carries the port, both pins and the frequency, and bus_shared is
// false. A second TwoWire(0) used to be created and begun here and then never
// read or written, which initialised I2C port 0 twice over the same pins.
static constexpr uint8_t BL_PWM_CHANNEL = 7;

// Write clock of the 8 bit panel bus. Left unset, LovyanGFX runs it at 16 MHz,
// which is 19 ms on the bus for one full 480x320 frame before a pixel has been
// rendered. The ST7796S datasheet asks for a write cycle of 66 ns (15 MHz) and
// 15 ns for each half of the pulse, so the default already sat on the limit
// and this is past it. 20 MHz is what nearly every published configuration of
// this board runs. A wrong pixel, a colour fringe or a shifted line, above all
// on a device that has warmed up, means going back down.
static constexpr uint32_t LCD_BUS_WRITE_HZ = 20000000;

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7796  _panel;
  lgfx::Bus_Parallel8 _bus;
  lgfx::Light_PWM     _light;
  lgfx::Touch_FT5x06  _touch;
public:
  LGFX(void) {
    { auto cfg = _bus.config();
      cfg.pin_wr=47; cfg.pin_rd=-1; cfg.pin_rs=0;
      cfg.pin_d0=9;  cfg.pin_d1=46; cfg.pin_d2=3;
      cfg.pin_d3=8;  cfg.pin_d4=18; cfg.pin_d5=17;
      cfg.pin_d6=16; cfg.pin_d7=15;
      cfg.freq_write=LCD_BUS_WRITE_HZ;
      _bus.config(cfg); _panel.setBus(&_bus); }
    { auto cfg = _panel.config();
      cfg.pin_cs=-1; cfg.pin_rst=4; cfg.pin_busy=-1;
      cfg.memory_width=320; cfg.memory_height=480;
      cfg.panel_width=320;  cfg.panel_height=480;
      cfg.invert=true; cfg.rgb_order=false;
      _panel.config(cfg); }
    { auto cfg = _light.config();
      // invert stays false: GPIO45 drives the WT32-SC01 Plus backlight
      // active-high, so duty 255 is full output. Flipping this to true does
      // not brighten a dim panel, it blacks it out -- and it would also turn
      // the backlight fully ON in deep sleep, where panelPrepareDeepSleep()
      // asks for brightness 0.
      cfg.pin_bl=hw_pins::LCD_BACKLIGHT; cfg.invert=false;
      cfg.freq=44100; cfg.pwm_channel=BL_PWM_CHANNEL;
      _light.config(cfg); _panel.setLight(&_light); }
    { auto cfg = _touch.config();
      cfg.x_min=0; cfg.x_max=319; cfg.y_min=0; cfg.y_max=479;
      cfg.pin_int=hw_pins::TOUCH_INT; cfg.bus_shared=false; cfg.offset_rotation=0;
      cfg.i2c_port=0; cfg.i2c_addr=0x38;
      cfg.pin_sda=hw_pins::TOUCH_SDA; cfg.pin_scl=hw_pins::TOUCH_SCL; cfg.freq=400000;
      _touch.config(cfg); _panel.setTouch(&_touch); }
    setPanel(&_panel);
  }
};

static LGFX tft;

bool panelBegin() {
  tft.init();  // brings up the touch I2C bus as configured in LGFX::_touch
  tft.setRotation(1);
  // Full output until displayPowerInit() applies the user's saved level. This
  // used to be a hardcoded 204, which meant every device ran the panel at 80%
  // while the settings slider sat at its 255 default and reported full
  // brightness -- the panel looked dim "at maximum" and dragging the slider to
  // a value it already held fired no LV_EVENT_VALUE_CHANGED to correct it.
  tft.setBrightness(BRIGHT_NORMAL_DEFAULT);
  tft.fillScreen(TFT_BLACK);
  return true;
}

void panelWrite(int32_t x, int32_t y, int32_t w, int32_t h, const uint16_t* px) {
  tft.startWrite();
  tft.setAddrWindow(x, y, w, h);
  tft.writePixels((const lgfx::rgb565_t*)px, w * h);
  tft.endWrite();
}

// The chip reports up to two points and the driver reads every one of them on
// each poll, so asking for the second costs no extra I2C traffic.
uint8_t panelTouchRead(PanelTouchPoint* points, uint8_t max_points) {
  static constexpr uint8_t CHIP_POINTS = 2;
  lgfx::touch_point_t tp[CHIP_POINTS];
  if (max_points > CHIP_POINTS) max_points = CHIP_POINTS;
  const uint8_t n = tft.getTouch(tp, max_points);
  for (uint8_t i = 0; i < n; i++) {
    points[i].x = tp[i].x;
    points[i].y = tp[i].y;
  }
  return n;
}

void panelSetBrightness(uint8_t brightness) {
  tft.setBrightness(brightness);
}

// Duty 0 still leaves LEDC driving the pin, and the backlight is an
// unregulated low-side switch. Hold the pin low instead.
void panelBacklightOff() {
  tft.setBrightness(0);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcDetach(hw_pins::LCD_BACKLIGHT);
#else
  ledcDetachPin(hw_pins::LCD_BACKLIGHT);
#endif
  pinMode(hw_pins::LCD_BACKLIGHT, OUTPUT);
  digitalWrite(hw_pins::LCD_BACKLIGHT, LOW);
}

void panelBacklightOn(uint8_t brightness) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  // Core 3 hands a pin to LEDC through its peripheral manager, and
  // LovyanGFX drives it by pin number. Its own init attaches it again.
  if (tft.light()) tft.light()->init(brightness);
#else
  ledcAttachPin(hw_pins::LCD_BACKLIGHT, BL_PWM_CHANNEL);
  tft.setBrightness(brightness);
#endif
}

void panelPrepareDeepSleep() {
  // setBrightness(0) leaves LEDC driving the pin. Detach it so the backlight
  // stays off while the chip is asleep.
  panelBacklightOff();
  // Trigger mode holds GPIO7 low for the whole touch. The Arduino pull-up
  // does not survive deep sleep; the RTC pull-up does, so the open-drain
  // line idles high and a finger is a real low.
  lgfx::i2c::writeRegister8(FT6336_I2C_PORT, FT6336_ADDR, FT6336_INTMODE_REG,
                            FT6336_INTMODE_TRIGGER, 0, 400000);
  pinMode(hw_pins::TOUCH_INT, INPUT_PULLUP);
  rtc_gpio_pullup_en((gpio_num_t)hw_pins::TOUCH_INT);
  rtc_gpio_pulldown_dis((gpio_num_t)hw_pins::TOUCH_INT);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)hw_pins::TOUCH_INT, 0);
}
