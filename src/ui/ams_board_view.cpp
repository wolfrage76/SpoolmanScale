#include "ams_board_view.h"

#include <esp_heap_caps.h>
#include <lvgl.h>

#include <new>

#include "ams_board_view_internal.h"
#include "lang.h"

// ============================================================
//  The AMS View page of the CrowPanel AMS monitor, drawn here.
//
//  Everything between the two marks below is that project's code
//  (github.com/wolfrage76/7in-Display-ESP32, firmware/src/ui/ui.cpp), cut
//  down to the one page and otherwise left as it stands there: its layout,
//  its swatches, its wording. That is on purpose: a change over there is
//  carried across by comparing the two files, so the words on the page are
//  that project's and not in the string table. Only the line shown while
//  nothing has arrived comes out of it.
//
//  The colours are not that project's. Its code still calls them by its own
//  names, COL_BG and the rest, but ams_board_view_internal.h points each name
//  at a colour of this firmware's theme, so the page follows the chosen
//  palette like every other screen.
//
//  What was changed to make it live here: the fonts are this firmware's
//  Montserrat, the unit table and the style cache are taken from PSRAM
//  instead of standing in internal RAM, and a tap on a card calls back
//  instead of opening the monitor's own unit page.
// ============================================================

namespace amsboard {

// Shared with ams_board_build.cpp, see ams_board_view_internal.h.
OverviewCard overviewCards[MAX_OVERVIEW_CARDS];
lv_obj_t *overviewRoot = nullptr;
lv_obj_t *overviewMain = nullptr;
lv_obj_t *overviewAux = nullptr;
lv_obj_t *overviewHint = nullptr;
lv_event_cb_t cardTapped = nullptr;

namespace {

// ---- from the CrowPanel AMS monitor ----------------------------------------

struct OverviewCardBind {
  int8_t src[MAX_OVERVIEW_SLOTS];
  uint8_t count = 0;
};

OverviewUnit *overviewUnits = nullptr;   // MAX_OVERVIEW_UNITS of them, in PSRAM
int overviewUnitCount = 0;
OverviewCardBind cardBinds[MAX_OVERVIEW_CARDS];

String connectionMessage;

uint32_t parseColor(const String &value) { return parseHexRgb(value); }
bool showRemainingGrams = false;

bool kindHas(const String &kind, const char *token) {
  String k = kind;
  k.toLowerCase();
  return k.indexOf(token) >= 0;
}

bool isHtKind(const String &kind) { return kindHas(kind, "ht"); }
bool isExtKind(const String &kind) { return kindHas(kind, "ext"); }

bool isAuxKind(const String &kind) {
  return isHtKind(kind) || isExtKind(kind);
}

int countMainOverview() {
  int n = 0;
  for (int i = 0; i < overviewUnitCount; ++i) {
    if (!isAuxKind(overviewUnits[i].amsKind)) {
      ++n;
    }
  }
  return n;
}

String bayLabelFor(const OverviewUnit &unit, const OverviewSlot &slot, int index) {
  if (slot.label.length()) {
    return slot.label;
  }
  String k = unit.amsKind;
  k.toLowerCase();
  if (k.indexOf("ht") >= 0) {
    return unit.label.length() ? unit.label : String("HT");
  }
  if (k.indexOf("ext") >= 0) {
    return String("Ext") + String(index + 1);
  }
  const char letter = (unit.amsId >= 0 && unit.amsId < 26) ? static_cast<char>('A' + unit.amsId) : 'A';
  return String(letter) + String(slot.slot + 1);
}


uint32_t spoolBorderColor(const OverviewSlot &slot) {
  if (slot.empty) {
    return COL_DIM;
  }
  if (slot.active) {
    return COL_ACCENT;
  }
  if (slot.remainingPercent >= 0 && slot.remainingPercent < SPOOL_CRITICAL_PERCENT) {
    return COL_DANGER;
  }
  if (slot.remainingPercent >= 0 && slot.remainingPercent < SPOOL_LOW_PERCENT) {
    return COL_WARN;
  }
  return COL_BORDER;
}

// AMS View bays are narrow, so one figure occupies the label and they trade
// places. A missing grams value stays on percent, so that bay never changes.
String remainingAmountText(int percent, int grams) {
  if (showRemainingGrams && grams >= 0) {
    return String(grams) + "g";
  }
  if (percent >= 0) {
    return String(percent) + "%";
  }
  if (grams >= 0) {
    return String(grams) + "g";
  }
  return "";
}

String remainingAmountText(int percent, const String &grams) {
  int g = -1;
  if (grams.length()) {
    g = grams.toInt();
  }
  return remainingAmountText(percent, g);
}


uint32_t remainingBarColor(int percent) {
  if (percent >= 0 && percent < SPOOL_CRITICAL_PERCENT) {
    return COL_DANGER;
  }
  if (percent >= 0 && percent < SPOOL_LOW_PERCENT) {
    return COL_WARN;
  }
  return COL_ACCENT;
}

void setLabelIfChanged(lv_obj_t *label, const String &text) {
  if (!label) {
    return;
  }
  const char *current = lv_label_get_text(label);
  if (current && text == current) {
    return;
  }
  lv_label_set_text(label, text.c_str());
}

// LVGL invalidates on every lv_obj_set_style_*/add_flag call regardless of
// whether the value actually changed, and flag/style changes force a relayout
// that repaints far more than the widget itself. Re-applying identical values
// each refresh cycle was repainting most of the screen, so cache and skip.
enum StyleProp : uint8_t {
  PROP_TEXT_COLOR = 0,
  PROP_BG_COLOR,
  PROP_BG_COLOR_INDICATOR,
  PROP_BG_OPA,
  PROP_BORDER_COLOR,
  PROP_BORDER_WIDTH,
  PROP_STYLE_TAG,
};

struct StyleCacheEntry {
  lv_obj_t *obj;
  uint8_t prop;
  uint32_t value;
};

// AMS View bays add a lot of styled widgets; undersizing caused every refresh
// to re-apply styles and invalidate large joined dirty regions.
constexpr size_t STYLE_CACHE_ENTRIES = 1024;
StyleCacheEntry *styleCache = nullptr;   // STYLE_CACHE_ENTRIES of them, in PSRAM
size_t styleCacheUsed = 0;

bool styleValueChanged(lv_obj_t *obj, uint8_t prop, uint32_t value) {
  for (size_t i = 0; i < styleCacheUsed; ++i) {
    StyleCacheEntry &entry = styleCache[i];
    if (entry.obj == obj && entry.prop == prop) {
      if (entry.value == value) {
        return false;
      }
      entry.value = value;
      return true;
    }
  }
  if (styleCache && styleCacheUsed < STYLE_CACHE_ENTRIES) {
    styleCache[styleCacheUsed++] = {obj, prop, value};
    return true;
  }
  // Cache full: still apply (safe) but this should not happen at 1024.
  return true;
}

void setTextColorIfChanged(lv_obj_t *obj, uint32_t color) {
  if (!obj || !styleValueChanged(obj, PROP_TEXT_COLOR, color)) {
    return;
  }
  lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}

void setBgColorIfChanged(lv_obj_t *obj, uint32_t color) {
  if (!obj || !styleValueChanged(obj, PROP_BG_COLOR, color)) {
    return;
  }
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
}

void setIndicatorColorIfChanged(lv_obj_t *obj, uint32_t color) {
  if (!obj || !styleValueChanged(obj, PROP_BG_COLOR_INDICATOR, color)) {
    return;
  }
  lv_obj_set_style_bg_color(obj, lv_color_hex(color), LV_PART_INDICATOR);
}

void setBgOpaIfChanged(lv_obj_t *obj, lv_opa_t opa) {
  if (!obj || !styleValueChanged(obj, PROP_BG_OPA, opa)) {
    return;
  }
  lv_obj_set_style_bg_opa(obj, opa, 0);
}

void setBorderColorIfChanged(lv_obj_t *obj, uint32_t color) {
  if (!obj || !styleValueChanged(obj, PROP_BORDER_COLOR, color)) {
    return;
  }
  lv_obj_set_style_border_color(obj, lv_color_hex(color), 0);
}

void setBorderWidthIfChanged(lv_obj_t *obj, lv_coord_t width) {
  if (!obj || !styleValueChanged(obj, PROP_BORDER_WIDTH, static_cast<uint32_t>(width))) {
    return;
  }
  lv_obj_set_style_border_width(obj, width, 0);
}

void setHiddenIfChanged(lv_obj_t *obj, bool hidden) {
  if (!obj || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == hidden) {
    return;
  }
  if (hidden) {
    lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }
}

void setBarValueIfChanged(lv_obj_t *bar, int32_t value) {
  if (!bar || lv_bar_get_value(bar) == value) {
    return;
  }
  lv_bar_set_value(bar, value, LV_ANIM_OFF);
}

// tag identifies which optional style is currently applied (0 = none).

void applyHeaderClimate(OverviewCard &card, const String &temperature, const String &humidity,
                        bool drying) {
  const bool showTemp = temperature.length() > 0;
  const bool showHum = humidity.length() > 0;
  setHiddenIfChanged(card.tempIcon, !showTemp);
  setHiddenIfChanged(card.tempText, !showTemp && !drying);
  if (card.tempText && (showTemp || drying)) {
    String text;
    if (drying) {
      text += "DRY";
    }
    if (showTemp) {
      if (text.length()) {
        text += " ";
      }
      text += temperature;
      text += "C";
    }
    setLabelIfChanged(card.tempText, text);
  }
  setHiddenIfChanged(card.humIcon, !showHum);
  setHiddenIfChanged(card.humText, !showHum);
  if (showHum && card.humText) {
    setLabelIfChanged(card.humText, humidity + "%");
  }
}

void applyBayClimate(lv_obj_t *label, const String &temperature, const String &humidity,
                    bool drying) {
  if (!label) {
    return;
  }
  String text;
  if (drying) {
    text += "DRY ";
  }
  if (temperature.length()) {
    text += temperature;
    text += "C";
  }
  if (humidity.length()) {
    if (text.length()) {
      text += " ";
    }
    text += humidity;
    text += "%";
  }
  setLabelIfChanged(label, text);
  setHiddenIfChanged(label, text.length() == 0);
}


void clearCardBinds() {
  for (int i = 0; i < MAX_OVERVIEW_CARDS; ++i) {
    cardBinds[i].count = 0;
    for (int s = 0; s < MAX_OVERVIEW_SLOTS; ++s) {
      cardBinds[i].src[s] = -1;
    }
  }
}

void pushCardUnit(int card, int unitIndex) {
  if (card < 0 || card >= MAX_OVERVIEW_CARDS) {
    return;
  }
  OverviewCardBind &b = cardBinds[card];
  if (b.count >= MAX_OVERVIEW_SLOTS) {
    return;
  }
  b.src[b.count++] = static_cast<int8_t>(unitIndex);
}

void rebuildOverviewBinds() {
  clearCardBinds();
  int htIdx[MAX_AMS_HT_UNITS];
  int htN = 0;
  int extIdx[4];
  int extN = 0;
  int mainCard = 0;
  for (int i = 0; i < overviewUnitCount; ++i) {
    if (isHtKind(overviewUnits[i].amsKind)) {
      if (htN < MAX_AMS_HT_UNITS) {
        htIdx[htN++] = i;
      }
    } else if (isExtKind(overviewUnits[i].amsKind)) {
      if (extN < 4) {
        extIdx[extN++] = i;
      }
    } else if (mainCard < AMS2PRO_COUNT) {
      pushCardUnit(mainCard++, i);
    }
  }

  int auxCard = AMS2PRO_COUNT;

  if (htN <= AMS2PRO_COUNT) {
    for (int i = 0; i < htN && auxCard < MAX_OVERVIEW_CARDS; ++i) {
      pushCardUnit(auxCard++, htIdx[i]);
    }
  } else {
    for (int i = 0; i < htN && auxCard < MAX_OVERVIEW_CARDS;) {
      int n = htN - i;
      if (n > MAX_OVERVIEW_SLOTS) {
        n = MAX_OVERVIEW_SLOTS;
      }
      const int card = auxCard++;
      for (int k = 0; k < n; ++k) {
        pushCardUnit(card, htIdx[i + k]);
      }
      i += n;
    }
  }

  for (int i = 0; i < extN && auxCard < MAX_OVERVIEW_CARDS; ++i) {
    pushCardUnit(auxCard++, extIdx[i]);
  }
}

int unitIndexForCard(int card) {
  if (card < 0 || card >= MAX_OVERVIEW_CARDS || cardBinds[card].count <= 0) {
    return -1;
  }
  return cardBinds[card].src[0];
}

bool cardUnitFor(int card, OverviewUnit &out) {
  if (card < 0 || card >= MAX_OVERVIEW_CARDS) {
    return false;
  }
  const OverviewCardBind &b = cardBinds[card];
  if (b.count <= 0) {
    return false;
  }
  if (b.count == 1) {
    const int idx = b.src[0];
    if (idx < 0 || idx >= overviewUnitCount) {
      return false;
    }
    out = overviewUnits[idx];
    return true;
  }

  out = OverviewUnit();
  out.amsKind = "ams_ht";
  String firstLabel;
  String lastLabel;
  for (int i = 0; i < b.count; ++i) {
    const int idx = b.src[i];
    if (idx < 0 || idx >= overviewUnitCount) {
      continue;
    }
    const OverviewUnit &u = overviewUnits[idx];
    if (!firstLabel.length()) {
      firstLabel = u.label;
    }
    lastLabel = u.label;
    OverviewSlot slot;
    if (u.slotCount > 0) {
      slot = u.slots[0];
    }
    if (!slot.label.length()) {
      slot.label = u.label.length() ? u.label : String("HT");
    }
    slot.temperature = u.temperature;
    slot.humidity = u.humidity;
    slot.drying = u.drying;
    if (out.slotCount < MAX_OVERVIEW_SLOTS) {
      out.slots[out.slotCount++] = slot;
    }
    if (u.drying) {
      out.drying = true;
    }
    if (!out.amsId) {
      out.amsId = u.amsId;
    }
  }
  if (firstLabel.length() && lastLabel.length() && firstLabel != lastLabel) {
    out.label = firstLabel + "-" + lastLabel;
  } else {
    out.label = firstLabel.length() ? firstLabel : String("AMS-HT");
  }
  return out.slotCount > 0;
}

int countAuxCards() {
  int n = 0;
  for (int i = AMS2PRO_COUNT; i < MAX_OVERVIEW_CARDS; ++i) {
    if (cardBinds[i].count > 0) {
      ++n;
    }
  }
  return n;
}

// The AMS cards, `count` of them shown, in their part of the page. One fills
// it. Two stand one above the other, each as wide as the page. Three and four
// make two rows of two. Bays stay side by side inside a card. `rowH` is the
// height of a row in pixels, 0 while the page has no height to share out yet.
void layoutMainCards(int count, int rowH) {
  const uint32_t cardW = count <= 2 ? 100 : 48;
  const uint32_t cardH = count <= 1 ? 100 : 48;
  for (int i = 0; i < AMS2PRO_COUNT; ++i) {
    lv_obj_t *card = overviewCards[i].root;
    if (!card) {
      continue;
    }
    const uint32_t tag = (cardW << 24) | (cardH << 16) | static_cast<uint32_t>(rowH);
    if (!styleValueChanged(card, PROP_STYLE_TAG, tag)) {
      continue;
    }
    lv_obj_set_flex_grow(card, 0);
    lv_obj_set_width(card, lv_pct(cardW));
    lv_obj_set_height(card, rowH > 0 ? rowH : lv_pct(cardH));
  }
}

void layoutOverviewBoard() {
  if (!overviewMain || !overviewAux) {
    return;
  }
  const int mainCount = countMainOverview();
  const int auxCount = countAuxCards();
  setHiddenIfChanged(overviewMain, mainCount <= 0);
  setHiddenIfChanged(overviewAux, auxCount <= 0);
  if (overviewHint) {
    const bool waiting = mainCount + auxCount <= 0;
    setHiddenIfChanged(overviewHint, !waiting);
    if (waiting) {
      setLabelIfChanged(overviewHint,
                        connectionMessage.length() ? connectionMessage : String(T(STR_AMSB_LINKING)));
    }
  }
  // More than one AMS goes onto two rows: a card is then half as high and,
  // with two of them, as wide as the page, which is what its four bays need.
  // AMS-HT and external spools stay together on the one row under them: a
  // unit there has a single bay, and four of those take the room of one AMS.
  const int mainRows = mainCount <= 0 ? 0 : (mainCount == 1 ? 1 : 2);
  const int rows = mainRows + (auxCount > 0 ? 1 : 0);

  // Every row is the same height, an AMS row or the one under them: the cards
  // show the same things. The height is in pixels, the page less the gaps
  // shared out, because percentages do not know about the gaps and left the
  // rows a few pixels apart in height. Height 0 plus flex grow never settled,
  // so the area under the header stayed black.
  lv_obj_set_flex_grow(overviewMain, 0);
  lv_obj_set_flex_grow(overviewAux, 0);
  lv_obj_update_layout(overviewRoot);
  const int gap = lv_obj_get_style_pad_row(overviewRoot, 0);
  const int pageH = lv_obj_get_content_height(overviewRoot);
  const int rowH = (rows > 0 && pageH > gap * rows) ? (pageH - gap * (rows - 1)) / rows : 0;
  if (rowH > 0) {
    lv_obj_set_height(overviewMain, mainRows > 0 ? mainRows * rowH + gap * (mainRows - 1) : 0);
    lv_obj_set_height(overviewAux, auxCount > 0 ? rowH : 0);
  } else if (rows > 0) {
    lv_obj_set_height(overviewMain, lv_pct(100 * mainRows / rows));
    lv_obj_set_height(overviewAux, lv_pct(auxCount > 0 ? 100 / rows : 0));
  }
  layoutMainCards(mainCount, rowH);
}

void refreshOverviewBay(OverviewBay &bay, const OverviewUnit &unit, int index, bool cellBay,
                        bool showSlotClimate) {
  if (!bay.root) {
    return;
  }
  if (index >= unit.slotCount) {
    setHiddenIfChanged(bay.root, true);
    return;
  }
  setHiddenIfChanged(bay.root, false);

  const OverviewSlot &slot = unit.slots[index];
  const int tag = (cellBay ? 2 : 0) | (unit.slotCount << 2) | (showSlotClimate ? 1 : 0);
  if (styleValueChanged(bay.root, PROP_STYLE_TAG, static_cast<uint32_t>(tag))) {
    if (cellBay) {
      lv_obj_set_flex_grow(bay.root, 0);
      lv_obj_set_width(bay.root, lv_pct(48));
      lv_obj_set_height(bay.root, lv_pct(48));
    } else {
      lv_obj_set_width(bay.root, LV_SIZE_CONTENT);
      lv_obj_set_flex_grow(bay.root, 1);
      lv_obj_set_height(bay.root, lv_pct(100));
    }
    if (bay.swatch) {
      // A thin bar on an AMS slot as on an AMS-HT one: the 16 pixels it gives
      // up are the second line of a colour name that does not fit on one.
      int swatchH = 22;
      if (cellBay && !isHtKind(unit.amsKind)) {
        swatchH = 28;
      }
      lv_obj_set_height(bay.swatch, swatchH);
    }
  }

  String title = bayLabelFor(unit, slot, index);
  setLabelIfChanged(bay.bayLabel, title);

  if (showSlotClimate) {
    applyBayClimate(bay.climate, slot.temperature, slot.humidity, slot.drying);
  } else {
    applyBayClimate(bay.climate, String(), String(), false);
  }

  if (slot.empty) {
    setBgColorIfChanged(bay.root, COL_AMS_CARD);
    setBorderColorIfChanged(bay.root, COL_DIM);
    setBorderWidthIfChanged(bay.root, 1);
    applyColorSwatch(bay.swatch, &bay.swatchFill, &bay.frost, &bay.highlight, &bay.overlay,
                     &bay.paint, bay.swatchSig, nullptr, 0, SwatchStyle::Flat, SwatchFinish::None,
                     SwatchSparkle::None, SwatchFiber::None, COL_AMS_SOFT, true);
    setTextColorIfChanged(bay.bayLabel, COL_MUTED);
    if (bay.bayLabel) {
      lv_obj_move_foreground(bay.bayLabel);
    }
    setLabelIfChanged(bay.material, String("Empty"));
    setTextColorIfChanged(bay.material, COL_MUTED);
    setHiddenIfChanged(bay.desc, true);
    setHiddenIfChanged(bay.remainingRow, true);
    setHiddenIfChanged(bay.brand, true);
    return;
  }

  const uint32_t color = slot.colorCount > 0 ? slot.colors[0] : parseColor(slot.color);
  const uint32_t ink = swatchInk(color);
  setBgColorIfChanged(bay.root, COL_AMS_SOFT);
  setBorderColorIfChanged(bay.root, spoolBorderColor(slot));
  setBorderWidthIfChanged(bay.root, slot.active ? 2 : 1);
  applyColorSwatch(bay.swatch, &bay.swatchFill, &bay.frost, &bay.highlight, &bay.overlay, &bay.paint,
                   bay.swatchSig, slot.colors, slot.colorCount, slot.colorStyle, slot.finish,
                   slot.sparkle, slot.fiber, color, false);
  setTextColorIfChanged(bay.bayLabel, ink);
  if (bay.bayLabel) {
    lv_obj_move_foreground(bay.bayLabel);
  }
  setLabelIfChanged(bay.material, slot.material.length() ? slot.material : String("--"));
  setTextColorIfChanged(bay.material, COL_TEXT);

  String desc = slot.colorName;
  if (slot.active) {
    if (desc.length()) {
      desc += " ";
    }
    desc += "FEEDING";
  }
  const bool showDesc = desc.length() > 0;
  setHiddenIfChanged(bay.desc, !showDesc);
  if (showDesc) {
    setLabelIfChanged(bay.desc, desc);
  }

  const bool showBrand = slot.manufacturer.length() > 0;
  setHiddenIfChanged(bay.brand, !showBrand);
  if (showBrand) {
    setLabelIfChanged(bay.brand, slot.manufacturer);
  }

  if (slot.remainingPercent >= 0) {
    setHiddenIfChanged(bay.remainingRow, false);
    int fill = slot.remainingPercent;
    if (fill > 100) {
      fill = 100;
    }
    setBarValueIfChanged(bay.remainingBar, fill);
    setIndicatorColorIfChanged(bay.remainingBar, remainingBarColor(slot.remainingPercent));
    setLabelIfChanged(bay.pct, remainingAmountText(slot.remainingPercent, slot.remainingGrams));
  } else {
    setHiddenIfChanged(bay.remainingRow, true);
  }
}

void refreshOverviewUnit(int index) {
  if (index < 0 || index >= MAX_OVERVIEW_CARDS) {
    return;
  }
  if (index == 0) {
    rebuildOverviewBinds();
    layoutOverviewBoard();
  }
  OverviewCard &card = overviewCards[index];
  if (!card.root) {
    return;
  }
  static OverviewUnit unit;
  if (!cardUnitFor(index, unit)) {
    setHiddenIfChanged(card.root, true);
    return;
  }

  card.amsId = unit.amsId;
  setHiddenIfChanged(card.root, false);

  String label = unit.label.length() ? unit.label : (String("AMS ") + String(unit.amsId));
  setLabelIfChanged(card.label, label);

  const bool groupedHt = cardBinds[index].count > 1;
  if (groupedHt) {
    applyHeaderClimate(card, String(), String(), unit.drying);
  } else {
    applyHeaderClimate(card, unit.temperature, unit.humidity, unit.drying);
  }

  // Regular AMS always lays its slots in one horizontal row. The 2×2 cell
  // layout is only for a grouped AMS-HT card that has more than two bays.
  const bool compactAux = isAuxKind(unit.amsKind) && unit.slotCount <= 2;
  const bool cellBay = index >= AMS2PRO_COUNT && !compactAux;
  for (int c = 0; c < MAX_OVERVIEW_SLOTS; ++c) {
    refreshOverviewBay(card.bays[c], unit, c, cellBay, groupedHt);
  }
}

void refreshOverviewPage() {
  rebuildOverviewBinds();
  for (int i = 0; i < MAX_OVERVIEW_CARDS; ++i) {
    refreshOverviewUnit(i);
  }
}


// AMS View only. The label width is fixed, so swapping % and g does not reflow.
void refreshRemainingLabelsOnly() {
  for (int i = 0; i < MAX_OVERVIEW_CARDS; ++i) {
    OverviewCard &card = overviewCards[i];
    if (!card.root || lv_obj_has_flag(card.root, LV_OBJ_FLAG_HIDDEN)) {
      continue;
    }
    OverviewUnit unit;
    if (!cardUnitFor(i, unit)) {
      continue;
    }
    for (int c = 0; c < unit.slotCount && c < MAX_OVERVIEW_SLOTS; ++c) {
      const OverviewSlot &slot = unit.slots[c];
      if (slot.remainingPercent < 0 && slot.remainingGrams < 0) {
        continue;
      }
      setLabelIfChanged(card.bays[c].pct, remainingAmountText(slot.remainingPercent, slot.remainingGrams));
    }
  }
}


// ---- end of the CrowPanel AMS monitor's code --------------------------------

// Both tables are too large for internal RAM, which this firmware keeps for
// WiFi and TLS: 11 kB of units and 12 kB of cache. Taken once and kept.
bool ensureTables() {
  if (!overviewUnits) {
    void *mem = heap_caps_calloc(MAX_OVERVIEW_UNITS, sizeof(OverviewUnit),
                                 MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!mem) {
      return false;
    }
    overviewUnits = static_cast<OverviewUnit *>(mem);
    for (int i = 0; i < MAX_OVERVIEW_UNITS; ++i) {
      new (overviewUnits + i) OverviewUnit();
    }
  }
  if (!styleCache) {
    styleCache = static_cast<StyleCacheEntry *>(heap_caps_calloc(
        STYLE_CACHE_ENTRIES, sizeof(StyleCacheEntry), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }
  return overviewUnits && styleCache;
}

}  // namespace

uint32_t viewBackgroundRgb() { return COL_BG; }
uint32_t viewTextRgb() { return COL_TEXT; }
uint32_t viewAccentRgb() { return COL_ACCENT; }
uint32_t viewMutedRgb() { return COL_MUTED; }

bool viewBuild(lv_obj_t *parent, lv_event_cb_t on_card) {
  viewForget();
  if (!ensureTables()) {
    return false;
  }
  cardTapped = on_card;
  buildOverviewPage(parent);
  if (!overviewRoot) {
    return false;
  }
  lv_obj_clear_flag(overviewRoot, LV_OBJ_FLAG_HIDDEN);
  refreshOverviewPage();
  return true;
}

void viewForget() {
  // The widgets went with their parent. Every pointer to them goes too, the
  // cache first: it knows widgets by address, and the next page's widgets
  // will stand at the same addresses.
  styleCacheUsed = 0;
  for (int i = 0; i < MAX_OVERVIEW_CARDS; ++i) {
    overviewCards[i] = OverviewCard();
  }
  overviewRoot = overviewMain = overviewAux = overviewHint = nullptr;
  overviewUnitCount = 0;
  cardTapped = nullptr;
}

void viewSetHint(const String &text) {
  connectionMessage = text;
  if (overviewRoot) {
    layoutOverviewBoard();
  }
}

void viewSetUnits(const OverviewUnit *units, int count) {
  if (!overviewRoot || !overviewUnits) {
    return;
  }
  if (count > MAX_OVERVIEW_UNITS) {
    count = MAX_OVERVIEW_UNITS;
  }
  for (int i = 0; i < count; ++i) {
    overviewUnits[i] = units[i];
  }
  overviewUnitCount = count;
  refreshOverviewPage();
}

void viewTogglePercentAndGrams() {
  if (!overviewRoot) {
    return;
  }
  showRemainingGrams = !showRemainingGrams;
  refreshRemainingLabelsOnly();
}

}  // namespace amsboard
