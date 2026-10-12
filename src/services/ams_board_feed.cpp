#include "services/ams_board_feed.h"

#include <ArduinoJson.h>
#include <esp_heap_caps.h>

#include <new>

#include "hardware/sd_logger.h"
#include "services/backend.h"
#include "services/filaman_api.h"

namespace amsboard {
namespace {

// Only what the board shows. The whole answer for a printer with four units
// is some 12 kB; this is what is left of it after the filter.
void buildFilter(JsonDocument &filter) {
  JsonObject p = filter["printers"].to<JsonArray>().add<JsonObject>();
  p["name"] = true;
  p["state"] = true;
  p["job"]["progress"] = true;

  JsonObject u = p["ams"].to<JsonArray>().add<JsonObject>();
  u["ams_id"] = true;
  u["kind"] = true;
  u["label"] = true;
  u["temperature"] = true;
  u["humidity"] = true;
  u["drying"]["status"] = true;

  JsonObject s = u["slots"].to<JsonArray>().add<JsonObject>();
  s["slot"] = true;
  s["empty"] = true;
  s["active"] = true;
  s["rfid"] = true;
  s["label"] = true;
  s["color"] = true;
  s["colors"] = true;
  s["color_style"] = true;
  s["finish"] = true;
  s["material"] = true;
  s["material_subgroup"] = true;
  s["color_name"] = true;
  s["filament"] = true;
  s["manufacturer"] = true;
  s["remaining_percent"] = true;
  s["remaining_grams"] = true;
}

// ---- from the CrowPanel AMS monitor, firmware/src/display_poll.cpp ---------
// The reading of the answer is that project's, so that both show the same
// thing for the same answer.

String jsonString(JsonVariantConst v) {
  if (v.isNull()) {
    return "";
  }
  if (v.is<const char *>()) {
    return String(v.as<const char *>());
  }
  if (v.is<bool>()) {
    return v.as<bool>() ? "true" : "false";
  }
  if (v.is<int>() || v.is<long>() || v.is<float>() || v.is<double>()) {
    return String(v.as<double>());
  }
  return "";
}

bool jsonBool(JsonVariantConst v, bool fallback = false) {
  if (v.isNull()) {
    return fallback;
  }
  if (v.is<bool>()) {
    return v.as<bool>();
  }
  if (v.is<int>()) {
    return v.as<int>() != 0;
  }
  String s = jsonString(v);
  s.toLowerCase();
  if (s == "true" || s == "1" || s == "on") {
    return true;
  }
  if (s == "false" || s == "0" || s == "off") {
    return false;
  }
  return fallback;
}

int jsonInt(JsonVariantConst v, int fallback = 0) {
  if (v.isNull()) {
    return fallback;
  }
  if (v.is<int>() || v.is<long>()) {
    return v.as<int>();
  }
  if (v.is<float>() || v.is<double>()) {
    return static_cast<int>(v.as<double>());
  }
  String s = jsonString(v);
  if (!s.length()) {
    return fallback;
  }
  return s.toInt();
}

String intStr(JsonVariantConst v) { return v.isNull() ? String("") : String(jsonInt(v)); }

void fillSwatchFromJson(JsonObjectConst s, const String &fallbackColor, uint32_t *outColors,
                        uint8_t &outCount, SwatchStyle &outStyle, SwatchFinish &outFinish) {
  outCount = 0;
  outStyle = SwatchStyle::Flat;
  outFinish = SwatchFinish::None;
  JsonArrayConst arr = s["colors"].as<JsonArrayConst>();
  if (!arr.isNull()) {
    for (JsonVariantConst c : arr) {
      if (outCount >= MAX_SLOT_COLORS) {
        break;
      }
      const String hex = jsonString(c);
      if (!hex.length()) {
        continue;
      }
      outColors[outCount++] = parseHexRgb(hex);
    }
  }
  if (outCount == 0 && fallbackColor.length()) {
    outColors[outCount++] = parseHexRgb(fallbackColor);
  }
  outStyle = parseSwatchStyle(jsonString(s["color_style"]), outCount);
  outFinish = parseSwatchFinish(jsonString(s["finish"]));
}

// The unit table is too big for the loop stack and too big for internal BSS.
// Keep it in PSRAM; constructors run once via placement new.
OverviewUnit *snapshotUnits() {
  static OverviewUnit *units = nullptr;
  if (units) {
    return units;
  }
  void *mem = heap_caps_calloc(MAX_OVERVIEW_UNITS, sizeof(OverviewUnit),
                               MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!mem) {
    return nullptr;
  }
  units = static_cast<OverviewUnit *>(mem);
  for (int i = 0; i < MAX_OVERVIEW_UNITS; ++i) {
    new (units + i) OverviewUnit();
  }
  return units;
}

int applySnapshot(JsonDocument &doc, OverviewUnit *units, PrinterLine *line) {
  JsonObjectConst p = doc["printers"][0].as<JsonObjectConst>();
  if (line) {
    line->name = jsonString(p["name"]);
    line->state = jsonString(p["state"]);
    line->progress = p["job"].isNull() ? -1 : jsonInt(p["job"]["progress"], -1);
  }

  int unitCount = 0;
  for (JsonObjectConst u : p["ams"].as<JsonArrayConst>()) {
    if (unitCount >= MAX_OVERVIEW_UNITS) {
      break;
    }
    const String kind = jsonString(u["kind"]);
    // The board is the AMS: a spool on the external holder is not in one.
    if (kind == "external") {
      continue;
    }
    const String panelKind =
        (kind == "ams_ht") ? "ams_ht" : (kind == "external" ? "external" : "ams2pro");
    const bool dryingActive = !u["drying"].isNull();

    OverviewUnit *ov = &units[unitCount++];
    *ov = OverviewUnit();
    ov->amsId = jsonInt(u["ams_id"], 0);
    ov->amsKind = kind.length() ? kind : panelKind;
    ov->label = jsonString(u["label"]);
    ov->temperature = intStr(u["temperature"]);
    ov->humidity = intStr(u["humidity"]);
    ov->drying = dryingActive;
    ov->slotCount = 0;

    for (JsonObjectConst s : u["slots"].as<JsonArrayConst>()) {
      if (ov->slotCount >= MAX_OVERVIEW_SLOTS) {
        break;
      }
      const bool empty = jsonBool(s["empty"], true);
      const bool active = jsonBool(s["active"], false);
      String color = jsonString(s["color"]);
      if (!color.length()) {
        color = "#202020";
      }
      const String material = jsonString(s["material"]);
      const String subgroup = jsonString(s["material_subgroup"]);

      OverviewSlot &os = ov->slots[ov->slotCount++];
      os.slot = jsonInt(s["slot"], 0);
      os.empty = empty;
      os.active = active && !empty;
      os.rfid = jsonBool(s["rfid"], false);
      os.color = color;
      if (empty) {
        os.colorCount = 0;
        os.colorStyle = SwatchStyle::Flat;
        os.finish = SwatchFinish::None;
      } else {
        fillSwatchFromJson(s, color, os.colors, os.colorCount, os.colorStyle, os.finish);
      }
      applySwatchOverlay(os.finish, os.sparkle, os.fiber, subgroup, material, empty);
      os.label = jsonString(s["label"]);
      os.material = material;
      os.colorName = jsonString(s["color_name"]);
      if (!os.colorName.length()) {
        os.colorName = jsonString(s["filament"]);
      }
      os.manufacturer = jsonString(s["manufacturer"]);
      os.remainingPercent = empty ? -1 : jsonInt(s["remaining_percent"], -1);
      os.remainingGrams = empty ? -1 : jsonInt(s["remaining_grams"], -1);
      if (kind == "ams_ht") {
        os.temperature = ov->temperature;
        os.humidity = ov->humidity;
        os.drying = dryingActive;
      }
    }
  }
  return unitCount;
}

// ---- end of the CrowPanel AMS monitor's code --------------------------------

}  // namespace

bool feedAvailable() { return backendMode() == BACKEND_FILAMAN; }

int feedFetch(int printer_id, const OverviewUnit **units, int *count,
              PrinterLine *line, uint32_t timeout_ms) {
  if (units) *units = nullptr;
  if (count) *count = 0;
  if (!feedAvailable() || printer_id <= 0) return -1;
  OverviewUnit *table = snapshotUnits();
  if (!table) return -3;

  char path[96];
  snprintf(path, sizeof(path), "/api/v1/display/printers/%d?fields=full", printer_id);
  JsonDocument filter;
  buildFilter(filter);
  JsonDocument doc;
  const int code = filamanGetJson(backendBaseUrl(), filamanApiKey(), path, doc, filter, timeout_ms);
  if (code != 200) return code;
  if (doc["printers"][0].isNull()) return -2;

  const int n = applySnapshot(doc, table, line);

  // What the answer carried, in one line. A bay without a colour name or an
  // amount looks the same on the page whether the server left it out or this
  // code lost it, and only the first is the server's to fix: FilaMan knows
  // either only for a bay it has a spool on file for.
  int bays = 0, filled = 0, with_amount = 0, with_name = 0, with_maker = 0;
  for (int i = 0; i < n; i++) {
    for (int k = 0; k < table[i].slotCount; k++) {
      const OverviewSlot &sl = table[i].slots[k];
      bays++;
      if (sl.empty) continue;
      filled++;
      if (sl.remainingPercent >= 0 || sl.remainingGrams >= 0) with_amount++;
      if (sl.colorName.length()) with_name++;
      if (sl.manufacturer.length()) with_maker++;
    }
  }
  static int last_sig = -1;
  const int sig = (((printer_id * 31 + n) * 31 + filled) * 31 + with_amount) * 31 + with_name;
  if (sig != last_sig) {
    last_sig = sig;
    logSDf("AMSBOARD: printer %d, %d unit(s), %d of %d bays filled: %d with an amount, "
           "%d with a colour name, %d with a maker",
           printer_id, n, filled, bays, with_amount, with_name, with_maker);
  }
  if (units) *units = table;
  if (count) *count = n;
  return 200;
}

}  // namespace amsboard
