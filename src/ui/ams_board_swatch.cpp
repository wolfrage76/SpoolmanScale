#include "ams_board_view_internal.h"

// The colour field of a bay on the AMS View page: one colour, stripes or a
// gradient, with the finishes FilaMan knows laid over it. Part of the
// CrowPanel AMS monitor's code, see ams_board_view.cpp.

namespace amsboard {
namespace {

uint32_t swatchSignature(const uint32_t *colors, uint8_t count, SwatchStyle style,
                         SwatchFinish finish, SwatchSparkle sparkle, SwatchFiber fiber,
                         uint32_t fallback) {
  uint32_t h = (static_cast<uint32_t>(count) << 16) | (static_cast<uint32_t>(style) << 8) |
               static_cast<uint32_t>(finish);
  h ^= (static_cast<uint32_t>(sparkle) << 4) | static_cast<uint32_t>(fiber);
  h ^= fallback * 2654435761u;
  for (uint8_t i = 0; i < count; ++i) {
    h ^= colors[i] * (0x9E3779B9u + i * 0x85EBCA6Bu);
  }
  return h;
}

uint32_t lerpChannel(uint32_t a, uint32_t b, int t /*0..256*/) {
  const int ai = static_cast<int>(a);
  const int bi = static_cast<int>(b);
  return static_cast<uint32_t>(ai + (((bi - ai) * t) >> 8));
}

uint32_t lerpRgb(uint32_t a, uint32_t b, int t /*0..256*/) {
  const uint32_t ar = (a >> 16) & 0xFF, ag = (a >> 8) & 0xFF, ab = a & 0xFF;
  const uint32_t br = (b >> 16) & 0xFF, bg = (b >> 8) & 0xFF, bb = b & 0xFF;
  return (lerpChannel(ar, br, t) << 16) | (lerpChannel(ag, bg, t) << 8) | lerpChannel(ab, bb, t);
}

uint32_t sampleSwatchGradient(const uint32_t *colors, uint8_t count, int t /*0..256*/) {
  if (count == 0) {
    return UI_COL_AMSB_NO_COLOUR;
  }
  if (count == 1 || t <= 0) {
    return colors[0];
  }
  if (t >= 256) {
    return colors[count - 1];
  }
  const int segments = count - 1;
  const int scaled = t * segments;
  const int idx = scaled >> 8;
  const int local = scaled & 0xFF;
  const uint8_t i0 = static_cast<uint8_t>(idx < segments ? idx : segments - 1);
  return lerpRgb(colors[i0], colors[i0 + 1], local);
}

void clearSwatchFillChildren(lv_obj_t *fill) {
  if (!fill) {
    return;
  }
  while (lv_obj_get_child_cnt(fill) > 0) {
    lv_obj_del(lv_obj_get_child(fill, 0));
  }
}

void swatchFillDrawCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) {
    return;
  }
  lv_obj_t *obj = lv_event_get_target(e);
  lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
  auto *paint = static_cast<SwatchPaint *>(lv_obj_get_user_data(obj));
  if (!paint || !draw_ctx || paint->count < 2 ||
      paint->style != static_cast<uint8_t>(SwatchStyle::Gradient)) {
    return;
  }

  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  lv_area_t visible;
  if (!_lv_area_intersect(&visible, &coords, draw_ctx->clip_area)) {
    return;
  }
  const int w = lv_area_get_width(&coords);
  const int h = lv_area_get_height(&coords);
  if (w <= 0 || h <= 0 || w > SCREEN_WIDTH || h > SCREEN_HEIGHT) {
    return;
  }

  // ~105° diagonal blend (FilaMan AMS View silk gradient). Only the clipped
  // span is painted; walking a broken full-screen width stalled the panel.
  const int denom = w * 4 + h;
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.bg_opa = LV_OPA_COVER;
  constexpr int STEP = 2;
  const int xStart = visible.x1 - coords.x1;
  const int xEnd = visible.x2 - coords.x1;
  for (int x = xStart - (xStart % STEP); x <= xEnd; x += STEP) {
    int t = ((x * 4 + h / 2) * 256) / (denom > 0 ? denom : 1);
    if (t < 0) {
      t = 0;
    }
    if (t > 256) {
      t = 256;
    }
    dsc.bg_color = lv_color_hex(sampleSwatchGradient(paint->colors, paint->count, t));
    lv_area_t strip = {static_cast<lv_coord_t>(coords.x1 + x), visible.y1,
                       static_cast<lv_coord_t>(coords.x1 + x + STEP - 1), visible.y2};
    if (strip.x2 > coords.x2) {
      strip.x2 = coords.x2;
    }
    lv_draw_rect(draw_ctx, &dsc, &strip);
  }
}

void drawSpeck(lv_draw_ctx_t *draw_ctx, lv_coord_t cx, lv_coord_t cy) {
  lv_draw_rect_dsc_t dsc;
  lv_draw_rect_dsc_init(&dsc);
  dsc.radius = LV_RADIUS_CIRCLE;
  dsc.border_width = 0;
  dsc.bg_color = lv_color_hex(UI_COL_AMSB_SPECK_RING);
  dsc.bg_opa = LV_OPA_60;
  lv_area_t ring = {static_cast<lv_coord_t>(cx - 2), static_cast<lv_coord_t>(cy - 2),
                    static_cast<lv_coord_t>(cx + 2), static_cast<lv_coord_t>(cy + 2)};
  lv_draw_rect(draw_ctx, &dsc, &ring);
  dsc.bg_color = lv_color_hex(UI_COL_AMSB_SHINE);
  dsc.bg_opa = LV_OPA_COVER;
  lv_area_t core = {static_cast<lv_coord_t>(cx - 1), static_cast<lv_coord_t>(cy - 1),
                    static_cast<lv_coord_t>(cx + 1), static_cast<lv_coord_t>(cy + 1)};
  lv_draw_rect(draw_ctx, &dsc, &core);
}

void swatchOverlayDrawCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) {
    return;
  }
  lv_obj_t *obj = lv_event_get_target(e);
  lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
  auto *paint = static_cast<SwatchPaint *>(lv_obj_get_user_data(obj));
  if (!paint || !draw_ctx || (paint->sparkle == 0 && paint->fiber == 0)) {
    return;
  }

  lv_area_t coords;
  lv_obj_get_coords(obj, &coords);
  lv_area_t visible;
  if (!_lv_area_intersect(&visible, &coords, draw_ctx->clip_area)) {
    return;
  }
  const int w = lv_area_get_width(&coords);
  const int h = lv_area_get_height(&coords);
  // The detail-page rail is a few pixels wide; ticks there read as noise.
  if (w < 24 || h < 8 || w > SCREEN_WIDTH || h > SCREEN_HEIGHT) {
    return;
  }

  if (paint->fiber != 0) {
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.width = 1;
    line.round_start = 0;
    line.round_end = 0;
    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 6; ++col) {
        const int x = coords.x1 + (col * w) / 6 + 2;
        const int y = coords.y1 + ((row * h) / 3) + (h / 2) + ((col & 1) ? h / 6 : 0);
        lv_point_t a = {static_cast<lv_coord_t>(x), static_cast<lv_coord_t>(y)};
        lv_point_t b = {static_cast<lv_coord_t>(x + 6), static_cast<lv_coord_t>(y - 4)};
        line.color = lv_color_hex(UI_COL_AMSB_SHINE);
        line.opa = LV_OPA_70;
        lv_draw_line(draw_ctx, &line, &a, &b);
        a.x = static_cast<lv_coord_t>(a.x + 3);
        a.y = static_cast<lv_coord_t>(a.y + 2);
        b.x = static_cast<lv_coord_t>(b.x + 3);
        b.y = static_cast<lv_coord_t>(b.y + 2);
        line.color = lv_color_hex(UI_COL_AMSB_SHADE);
        line.opa = LV_OPA_40;
        lv_draw_line(draw_ctx, &line, &a, &b);
      }
    }
  }

  if (paint->sparkle != 0) {
    // Same pinpoint layout FilaMan uses. Glitter adds a tighter second set.
    // Positions are percentages so a short AMS bar still shows the specks.
    static const int8_t specks[][2] = {
        {22, 68}, {62, 28}, {40, 42}, {80, 60}, {14, 36}, {48, 82}, {86, 18}, {32, 16},
    };
    const int count = paint->sparkle == static_cast<uint8_t>(SwatchSparkle::Glitter) ? 8 : 5;
    for (int i = 0; i < count; ++i) {
      const lv_coord_t cx = static_cast<lv_coord_t>(coords.x1 + (w * specks[i][0]) / 100);
      const lv_coord_t cy = static_cast<lv_coord_t>(coords.y1 + (h * specks[i][1]) / 100);
      drawSpeck(draw_ctx, cx, cy);
    }
  }
}

void ensureSwatchLayers(lv_obj_t *swatch, lv_obj_t **fillOut, lv_obj_t **frostOut,
                        lv_obj_t **highlightOut, lv_obj_t **overlayOut) {
  if (!swatch) {
    return;
  }
  if (!*fillOut) {
    *fillOut = lv_obj_create(swatch);
    lv_obj_remove_style_all(*fillOut);
    lv_obj_set_size(*fillOut, lv_pct(100), lv_pct(100));
    lv_obj_align(*fillOut, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(*fillOut, LV_OPA_COVER, 0);
    lv_obj_clear_flag(*fillOut, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(*fillOut, LV_OBJ_FLAG_SCROLLABLE);
    // The swatch is a flex row for its label. A 100% color layer in that row
    // is measured as another full-width item and the layout never settles,
    // so nothing under the header gets drawn.
    lv_obj_add_flag(*fillOut, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_move_background(*fillOut);
    lv_obj_add_event_cb(*fillOut, swatchFillDrawCb, LV_EVENT_DRAW_MAIN, nullptr);
  }
  if (!*frostOut) {
    *frostOut = lv_obj_create(swatch);
    lv_obj_remove_style_all(*frostOut);
    lv_obj_set_size(*frostOut, lv_pct(100), lv_pct(100));
    lv_obj_align(*frostOut, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(*frostOut, lv_color_hex(UI_COL_AMSB_SHINE), 0);
    lv_obj_set_style_bg_opa(*frostOut, LV_OPA_20, 0);
    lv_obj_clear_flag(*frostOut, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(*frostOut, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(*frostOut, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(*frostOut, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 5; ++i) {
      lv_obj_t *line = lv_obj_create(*frostOut);
      lv_obj_remove_style_all(line);
      lv_obj_set_size(line, lv_pct(100), 1);
      lv_obj_set_pos(line, 0, 3 + i * 6);
      lv_obj_set_style_bg_color(line, lv_color_hex(UI_COL_AMSB_SHINE), 0);
      lv_obj_set_style_bg_opa(line, LV_OPA_40, 0);
      lv_obj_clear_flag(line, LV_OBJ_FLAG_CLICKABLE);
    }
  }
  if (!*highlightOut) {
    *highlightOut = lv_obj_create(swatch);
    lv_obj_remove_style_all(*highlightOut);
    lv_obj_set_size(*highlightOut, lv_pct(70), lv_pct(55));
    lv_obj_align(*highlightOut, LV_ALIGN_TOP_LEFT, 0, 0);
    // Soft glass corner wash (FilaMan uses a diagonal white fade).
    lv_obj_set_style_bg_color(*highlightOut, lv_color_hex(UI_COL_AMSB_SHINE), 0);
    lv_obj_set_style_bg_opa(*highlightOut, LV_OPA_40, 0);
    lv_obj_clear_flag(*highlightOut, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(*highlightOut, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(*highlightOut, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(*highlightOut, LV_OBJ_FLAG_HIDDEN);
  }
  if (overlayOut && !*overlayOut) {
    *overlayOut = lv_obj_create(swatch);
    lv_obj_remove_style_all(*overlayOut);
    lv_obj_set_size(*overlayOut, lv_pct(100), lv_pct(100));
    lv_obj_align(*overlayOut, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_opa(*overlayOut, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(*overlayOut, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(*overlayOut, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(*overlayOut, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_add_flag(*overlayOut, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(*overlayOut, swatchOverlayDrawCb, LV_EVENT_DRAW_MAIN, nullptr);
  }
}

}  // namespace

uint32_t swatchInk(uint32_t rgb) {
  const int r = (rgb >> 16) & 0xFF;
  const int g = (rgb >> 8) & 0xFF;
  const int b = rgb & 0xFF;
  // WCAG relative-luminance crossover vs black/white (~L = 0.179).
  const int y = r * 2126 + g * 7152 + b * 722;
  return y > 456450 ? COL_AMS_INK_DARK : COL_AMS_INK_LIGHT;
}

void applyColorSwatch(lv_obj_t *swatch, lv_obj_t **fillOut, lv_obj_t **frostOut,
                      lv_obj_t **highlightOut, lv_obj_t **overlayOut, SwatchPaint *paint,
                      uint32_t &sig, const uint32_t *colors, uint8_t count, SwatchStyle style,
                      SwatchFinish finish, SwatchSparkle sparkle, SwatchFiber fiber,
                      uint32_t fallback, bool empty) {
  if (!swatch || !paint) {
    return;
  }
  ensureSwatchLayers(swatch, fillOut, frostOut, highlightOut, overlayOut);
  lv_obj_t *fill = *fillOut;
  lv_obj_t *frost = *frostOut;
  lv_obj_t *highlight = *highlightOut;

  uint32_t useColors[MAX_SLOT_COLORS];
  uint8_t useCount = 0;
  SwatchStyle useStyle = SwatchStyle::Flat;
  SwatchFinish useFinish = SwatchFinish::None;
  if (!empty) {
    useCount = count > MAX_SLOT_COLORS ? MAX_SLOT_COLORS : count;
    for (uint8_t i = 0; i < useCount; ++i) {
      useColors[i] = colors[i];
    }
    if (useCount == 0) {
      useColors[0] = fallback;
      useCount = 1;
    }
    useStyle = (useCount > 1) ? style : SwatchStyle::Flat;
    if (useCount > 1 && useStyle == SwatchStyle::Flat) {
      useStyle = SwatchStyle::Striped;
    }
    useFinish = finish;
  } else {
    useColors[0] = fallback;
    useCount = 1;
  }

  const SwatchSparkle useSparkle = empty ? SwatchSparkle::None : sparkle;
  const SwatchFiber useFiber = empty ? SwatchFiber::None : fiber;
  const uint32_t nextSig =
      swatchSignature(useColors, useCount, useStyle, useFinish, useSparkle, useFiber, fallback);
  if (nextSig != sig) {
    sig = nextSig;
    paint->count = useCount;
    paint->style = static_cast<uint8_t>(useStyle);
    paint->sparkle = static_cast<uint8_t>(useSparkle);
    paint->fiber = static_cast<uint8_t>(useFiber);
    for (uint8_t i = 0; i < useCount; ++i) {
      paint->colors[i] = useColors[i];
    }

    clearSwatchFillChildren(fill);
    lv_obj_set_user_data(fill, paint);

    if (useStyle == SwatchStyle::Striped && useCount > 1) {
      lv_obj_set_style_bg_opa(fill, LV_OPA_TRANSP, 0);
      lv_obj_set_flex_flow(fill, LV_FLEX_FLOW_ROW);
      lv_obj_set_style_pad_all(fill, 0, 0);
      lv_obj_set_style_pad_column(fill, 0, 0);
      for (uint8_t i = 0; i < useCount; ++i) {
        lv_obj_t *band = lv_obj_create(fill);
        lv_obj_remove_style_all(band);
        lv_obj_set_height(band, lv_pct(100));
        lv_obj_set_flex_grow(band, 1);
        lv_obj_set_style_bg_opa(band, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(band, lv_color_hex(useColors[i]), 0);
        lv_obj_clear_flag(band, LV_OBJ_FLAG_CLICKABLE);
      }
    } else if (useStyle == SwatchStyle::Gradient && useCount > 1) {
      lv_obj_set_style_bg_opa(fill, LV_OPA_TRANSP, 0);
      lv_obj_invalidate(fill);
    } else {
      lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
      lv_obj_set_style_bg_color(fill, lv_color_hex(useColors[0]), 0);
    }

    lv_obj_set_style_bg_opa(swatch, LV_OPA_TRANSP, 0);

    if (overlayOut && *overlayOut) {
      lv_obj_set_user_data(*overlayOut, paint);
      if (useSparkle == SwatchSparkle::None && useFiber == SwatchFiber::None) {
        lv_obj_add_flag(*overlayOut, LV_OBJ_FLAG_HIDDEN);
      } else {
        lv_obj_clear_flag(*overlayOut, LV_OBJ_FLAG_HIDDEN);
        lv_obj_move_foreground(*overlayOut);
        lv_obj_invalidate(*overlayOut);
      }
    }
  }

  if (useFinish == SwatchFinish::Translucent) {
    lv_obj_clear_flag(frost, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(highlight, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(frost);
    lv_obj_move_foreground(highlight);
    if (overlayOut && *overlayOut && !lv_obj_has_flag(*overlayOut, LV_OBJ_FLAG_HIDDEN)) {
      lv_obj_move_foreground(*overlayOut);
    }
  } else {
    lv_obj_add_flag(frost, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(highlight, LV_OBJ_FLAG_HIDDEN);
  }

  if (useFinish == SwatchFinish::Glow) {
    lv_obj_set_style_border_width(swatch, 1, 0);
    lv_obj_set_style_border_color(swatch, lv_color_hex(UI_COL_AMSB_SHADE), 0);
    lv_obj_set_style_border_opa(swatch, LV_OPA_70, 0);
    lv_obj_set_style_outline_width(swatch, 4, 0);
    lv_obj_set_style_outline_color(swatch, lv_color_hex(COL_GLOW_FRAME), 0);
    lv_obj_set_style_outline_opa(swatch, LV_OPA_COVER, 0);
    lv_obj_set_style_outline_pad(swatch, 0, 0);
  } else {
    lv_obj_set_style_outline_width(swatch, 0, 0);
    lv_obj_set_style_border_width(swatch, 0, 0);
  }
}

}  // namespace amsboard
