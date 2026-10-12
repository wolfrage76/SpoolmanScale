#pragma once

#include <Arduino.h>

#include "ui/theme.h"

// ============================================================
//  What the AMS board draws, see ui/ams_board.h.
//
//  Taken from the CrowPanel AMS monitor (github.com/wolfrage76/
//  7in-Display-ESP32, firmware/include/models.h) and kept in its own words,
//  so that a change over there can be carried across by comparing the two
//  files. Only what the AMS View page needs came along.
// ============================================================

namespace amsboard {

constexpr int MAX_OVERVIEW_UNITS = 16;  // 4 AMS + 8 AMS-HT + extras
constexpr int MAX_OVERVIEW_CARDS = 8;   // 4 AMS + 4 aux (separate HT or grouped + ext)
constexpr int MAX_OVERVIEW_SLOTS = 4;
constexpr int MAX_AMS_HT_UNITS = 8;
constexpr int MAX_SLOT_COLORS = 4;  // FilaMan multi-color (silk dual / gradient / striped)

// Matches FilaMan AMS View: multi_color_style + finish_type on the filament.
enum class SwatchStyle : uint8_t { Flat = 0, Striped = 1, Gradient = 2 };
enum class SwatchFinish : uint8_t { None = 0, Translucent = 1, Glow = 2 };
// FilaMan AMS View: subgroup galaxy/sparkle/glitter, and cf/gf on the subgroup or material.
enum class SwatchSparkle : uint8_t { None = 0, Galaxy = 1, Sparkle = 2, Glitter = 3 };
enum class SwatchFiber : uint8_t { None = 0, Cf = 1, Gf = 2 };

struct OverviewSlot {
  int slot = 0;
  bool empty = true;
  bool active = false;
  bool rfid = false;
  bool drying = false;
  String color = "#202020";
  uint32_t colors[MAX_SLOT_COLORS] = {};
  uint8_t colorCount = 0;
  SwatchStyle colorStyle = SwatchStyle::Flat;
  SwatchFinish finish = SwatchFinish::None;
  SwatchSparkle sparkle = SwatchSparkle::None;
  SwatchFiber fiber = SwatchFiber::None;
  String label;
  String material;
  String colorName;
  String manufacturer;
  String temperature;
  String humidity;
  int remainingPercent = -1;
  int remainingGrams = -1;
};

struct OverviewUnit {
  int amsId = 0;
  String amsKind;
  String label;
  String temperature;
  String humidity;
  bool drying = false;
  int slotCount = 0;
  OverviewSlot slots[MAX_OVERVIEW_SLOTS];
};

// What the header says about the printer.
struct PrinterLine {
  String name;
  String state;
  int progress = -1;
};

// Parse #RRGGBB / RRGGBBAA into 0xRRGGBB. Invalid → UI_COL_AMSB_NO_COLOUR.
inline uint32_t parseHexRgb(const String &value) {
  String text = value;
  text.trim();
  if (text.startsWith("#")) {
    text.remove(0, 1);
  }
  if (text.length() < 6) {
    return UI_COL_AMSB_NO_COLOUR;
  }
  return static_cast<uint32_t>(strtoul(text.substring(0, 6).c_str(), nullptr, 16));
}

inline SwatchStyle parseSwatchStyle(const String &raw, uint8_t colorCount) {
  if (colorCount < 2) {
    return SwatchStyle::Flat;
  }
  String s = raw;
  s.trim();
  s.toLowerCase();
  if (s == "gradient") {
    return SwatchStyle::Gradient;
  }
  // FilaMan defaults multi without an explicit style to striped.
  return SwatchStyle::Striped;
}

inline SwatchFinish parseSwatchFinish(const String &raw) {
  String s = raw;
  s.trim();
  s.toLowerCase();
  if (s == "glow") {
    return SwatchFinish::Glow;
  }
  if (s == "translucent") {
    return SwatchFinish::Translucent;
  }
  // Display API also sends "neon" for the chartreuse rim.
  if (s == "neon") {
    return SwatchFinish::Glow;
  }
  return SwatchFinish::None;
}

// Whole tokens split on anything that is not a letter or digit. "PA6-CF" yields cf.
inline bool swatchHasToken(const String &raw, const char *word) {
  String text = raw;
  text.toLowerCase();
  const int wordLen = static_cast<int>(strlen(word));
  const int n = text.length();
  int i = 0;
  while (i < n) {
    while (i < n) {
      const char c = text.charAt(i);
      const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
      if (alnum) {
        break;
      }
      ++i;
    }
    const int start = i;
    while (i < n) {
      const char c = text.charAt(i);
      const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
      if (!alnum) {
        break;
      }
      ++i;
    }
    if (i > start && (i - start) == wordLen && text.substring(start, i) == word) {
      return true;
    }
  }
  return false;
}

inline SwatchSparkle parseSwatchSparkle(const String &subgroup) {
  String s = subgroup;
  s.trim();
  s.toLowerCase();
  if (s == "glitter") {
    return SwatchSparkle::Glitter;
  }
  if (s == "sparkle") {
    return SwatchSparkle::Sparkle;
  }
  if (s == "galaxy") {
    return SwatchSparkle::Galaxy;
  }
  return SwatchSparkle::None;
}

inline SwatchFiber parseSwatchFiber(const String &subgroup, const String &material) {
  if (swatchHasToken(subgroup, "cf") || swatchHasToken(material, "cf")) {
    return SwatchFiber::Cf;
  }
  if (swatchHasToken(subgroup, "gf") || swatchHasToken(material, "gf")) {
    return SwatchFiber::Gf;
  }
  return SwatchFiber::None;
}

inline void applySwatchOverlay(SwatchFinish &finish, SwatchSparkle &sparkle, SwatchFiber &fiber,
                               const String &subgroup, const String &material, bool empty) {
  sparkle = SwatchSparkle::None;
  fiber = SwatchFiber::None;
  if (empty) {
    return;
  }
  if (finish == SwatchFinish::None) {
    finish = parseSwatchFinish(subgroup);
  }
  sparkle = parseSwatchSparkle(subgroup);
  fiber = parseSwatchFiber(subgroup, material);
}

}  // namespace amsboard
