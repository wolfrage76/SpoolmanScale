#include "ams_board_view_internal.h"

#include "lang.h"

// The widgets of the AMS View page, built once: the cards, their bays and
// the two small icons of a card's header. Part of the CrowPanel AMS monitor's
// code, see ams_board_view.cpp.

namespace amsboard {
namespace {

// 8x10 ALPHA_8BIT glyphs, recolored at draw time.
constexpr int ICON_W = 8;
constexpr int ICON_H = 10;
const uint8_t ICON_THERMO_MAP[ICON_W * ICON_H] = {
    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00,
    0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
};
const uint8_t ICON_DROP_MAP[ICON_W * ICON_H] = {
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00,
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00,
};
lv_img_dsc_t iconThermo;
lv_img_dsc_t iconDrop;
bool climateIconsReady = false;

void initImgIcon(lv_img_dsc_t &dsc, const uint8_t *data) {
  dsc.header.cf = LV_IMG_CF_ALPHA_8BIT;
  dsc.header.always_zero = 0;
  dsc.header.reserved = 0;
  dsc.header.w = ICON_W;
  dsc.header.h = ICON_H;
  dsc.data_size = ICON_W * ICON_H;
  dsc.data = data;
}

void ensureClimateIcons() {
  if (climateIconsReady) {
    return;
  }
  initImgIcon(iconThermo, ICON_THERMO_MAP);
  initImgIcon(iconDrop, ICON_DROP_MAP);
  climateIconsReady = true;
}

lv_obj_t *makeLabel(lv_obj_t *parent, const lv_font_t *font, lv_color_t color) {
  lv_obj_t *label = lv_label_create(parent);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color, 0);
  lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
  lv_obj_set_width(label, lv_pct(100));
  return label;
}

lv_obj_t *makeClimateIcon(lv_obj_t *parent, const lv_img_dsc_t *src) {
  lv_obj_t *img = lv_img_create(parent);
  lv_img_set_src(img, src);
  lv_obj_set_style_img_recolor(img, lv_color_hex(COL_MUTED), 0);
  lv_obj_set_style_img_recolor_opa(img, LV_OPA_COVER, 0);
  lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
  return img;
}

void overviewCardEvent(lv_event_t *event) {
  if (lv_event_get_code(event) != LV_EVENT_CLICKED) {
    return;
  }
  if (cardTapped) {
    cardTapped(event);
  }
}

void passClicksToParent(lv_obj_t *obj) {
  const uint32_t count = lv_obj_get_child_cnt(obj);
  for (uint32_t i = 0; i < count; ++i) {
    lv_obj_t *child = lv_obj_get_child(obj, i);
    lv_obj_clear_flag(child, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(child, LV_OBJ_FLAG_SCROLLABLE);
    passClicksToParent(child);
  }
}

void buildOverviewBay(OverviewBay &bay, lv_obj_t *parent, bool withClimate) {
  bay.root = lv_obj_create(parent);
  lv_obj_remove_style_all(bay.root);
  lv_obj_set_style_bg_opa(bay.root, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(bay.root, lv_color_hex(COL_AMS_SOFT), 0);
  lv_obj_set_style_border_width(bay.root, 1, 0);
  lv_obj_set_style_border_color(bay.root, lv_color_hex(COL_BORDER), 0);
  lv_obj_set_style_radius(bay.root, 10, 0);
  lv_obj_set_style_min_width(bay.root, 0, 0);
  lv_obj_set_flex_flow(bay.root, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_grow(bay.root, 1);
  lv_obj_set_height(bay.root, lv_pct(100));
  lv_obj_clear_flag(bay.root, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(bay.root, LV_OBJ_FLAG_CLICKABLE);

  bay.swatch = lv_obj_create(bay.root);
  lv_obj_remove_style_all(bay.swatch);
  lv_obj_set_width(bay.swatch, lv_pct(100));
  lv_obj_set_height(bay.swatch, 34);
  lv_obj_set_style_bg_opa(bay.swatch, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_hor(bay.swatch, 6, 0);
  lv_obj_set_style_pad_ver(bay.swatch, 4, 0);
  lv_obj_set_style_clip_corner(bay.swatch, true, 0);
  lv_obj_set_flex_flow(bay.swatch, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bay.swatch, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(bay.swatch, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(bay.swatch, LV_OBJ_FLAG_CLICKABLE);

  bay.bayLabel = lv_label_create(bay.swatch);
  lv_obj_set_style_text_font(bay.bayLabel, &lv_font_montserrat_ext_12, 0);
  lv_obj_set_style_text_color(bay.bayLabel, lv_color_hex(COL_TEXT), 0);
  lv_label_set_long_mode(bay.bayLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_width(bay.bayLabel, lv_pct(100));
  // Inset the slot id only. Padding the swatch also shifts the color fill.
  lv_obj_set_style_pad_left(bay.bayLabel, 10, 0);
  lv_obj_move_foreground(bay.bayLabel);

  bay.material = makeLabel(bay.root, &lv_font_montserrat_ext_14, lv_color_hex(COL_TEXT));
  lv_obj_set_style_pad_hor(bay.material, 6, 0);
  bay.desc = makeLabel(bay.root, &lv_font_montserrat_ext_12, lv_color_hex(COL_MUTED));
  lv_obj_set_style_pad_hor(bay.desc, 6, 0);
  lv_label_set_long_mode(bay.desc, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(bay.desc, lv_pct(100));
  lv_obj_set_flex_grow(bay.desc, 1);

  if (withClimate) {
    bay.climate = makeLabel(bay.root, &lv_font_montserrat_ext_12, lv_color_hex(COL_MUTED));
    lv_obj_set_style_pad_hor(bay.climate, 6, 0);
    lv_obj_add_flag(bay.climate, LV_OBJ_FLAG_HIDDEN);
  }

  bay.brand = makeLabel(bay.root, &lv_font_montserrat_ext_12, lv_color_hex(COL_DIM));
  lv_obj_set_style_pad_hor(bay.brand, 6, 0);
  lv_label_set_long_mode(bay.brand, LV_LABEL_LONG_DOT);
  lv_obj_set_width(bay.brand, lv_pct(100));
  lv_obj_add_flag(bay.brand, LV_OBJ_FLAG_HIDDEN);

  bay.remainingRow = lv_obj_create(bay.root);
  lv_obj_remove_style_all(bay.remainingRow);
  lv_obj_set_width(bay.remainingRow, lv_pct(100));
  lv_obj_set_height(bay.remainingRow, 12);
  lv_obj_set_style_pad_hor(bay.remainingRow, 6, 0);
  lv_obj_set_flex_flow(bay.remainingRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(bay.remainingRow, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                       LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_column(bay.remainingRow, 4, 0);
  lv_obj_clear_flag(bay.remainingRow, LV_OBJ_FLAG_SCROLLABLE);

  bay.remainingBar = lv_bar_create(bay.remainingRow);
  lv_obj_set_flex_grow(bay.remainingBar, 1);
  lv_obj_set_height(bay.remainingBar, 4);
  lv_bar_set_range(bay.remainingBar, 0, 100);
  lv_obj_set_style_bg_color(bay.remainingBar, lv_color_hex(COL_BAR_TRACK), LV_PART_MAIN);
  lv_obj_set_style_bg_color(bay.remainingBar, lv_color_hex(COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_radius(bay.remainingBar, 4, LV_PART_MAIN);
  lv_obj_set_style_radius(bay.remainingBar, 4, LV_PART_INDICATOR);

  bay.pct = lv_label_create(bay.remainingRow);
  lv_obj_set_style_text_font(bay.pct, &lv_font_montserrat_ext_12, 0);
  lv_obj_set_style_text_color(bay.pct, lv_color_hex(COL_MUTED), 0);
  // Fixed width so "100%" and "1000g" swap without resizing the bar.
  lv_obj_set_width(bay.pct, 48);
  lv_obj_set_style_text_align(bay.pct, LV_TEXT_ALIGN_RIGHT, 0);
  lv_label_set_text(bay.pct, "");
}

}  // namespace

void buildOverviewPage(lv_obj_t *parent) {
  overviewRoot = lv_obj_create(parent);
  lv_obj_remove_style_all(overviewRoot);
  lv_obj_set_width(overviewRoot, lv_pct(100));
  lv_obj_set_flex_grow(overviewRoot, 1);
  lv_obj_set_flex_flow(overviewRoot, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(overviewRoot, 6, 0);
  lv_obj_clear_flag(overviewRoot, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(overviewRoot, LV_OBJ_FLAG_HIDDEN);

  overviewHint = lv_label_create(overviewRoot);
  lv_obj_set_width(overviewHint, lv_pct(100));
  lv_obj_set_style_text_font(overviewHint, &lv_font_montserrat_ext_16, 0);
  lv_obj_set_style_text_color(overviewHint, lv_color_hex(COL_MUTED), 0);
  lv_obj_set_style_text_align(overviewHint, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_pad_top(overviewHint, 24, 0);
  lv_label_set_text(overviewHint, T(STR_AMSB_LINKING));

  overviewMain = lv_obj_create(overviewRoot);
  lv_obj_remove_style_all(overviewMain);
  lv_obj_set_width(overviewMain, lv_pct(100));
  lv_obj_set_height(overviewMain, lv_pct(100));
  lv_obj_set_flex_grow(overviewMain, 0);
  lv_obj_set_flex_flow(overviewMain, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(overviewMain, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_column(overviewMain, 8, 0);
  // The same gap as between the rows of the page, see layoutOverviewBoard.
  lv_obj_set_style_pad_row(overviewMain, 6, 0);
  lv_obj_clear_flag(overviewMain, LV_OBJ_FLAG_SCROLLABLE);

  overviewAux = lv_obj_create(overviewRoot);
  lv_obj_remove_style_all(overviewAux);
  lv_obj_set_width(overviewAux, lv_pct(100));
  lv_obj_set_height(overviewAux, 0);
  lv_obj_set_flex_grow(overviewAux, 0);
  lv_obj_set_flex_flow(overviewAux, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_column(overviewAux, 6, 0);
  lv_obj_clear_flag(overviewAux, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < MAX_OVERVIEW_CARDS; ++i) {
    OverviewCard &card = overviewCards[i];
    lv_obj_t *row = (i < AMS2PRO_COUNT) ? overviewMain : overviewAux;
    card.root = lv_obj_create(row);
    lv_obj_remove_style_all(card.root);
    lv_obj_set_style_bg_opa(card.root, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(card.root, lv_color_hex(COL_AMS_CARD), 0);
    lv_obj_set_style_border_width(card.root, 1, 0);
    lv_obj_set_style_border_color(card.root, lv_color_hex(COL_AMS_LINE), 0);
    lv_obj_set_style_radius(card.root, 12, 0);
    lv_obj_set_style_pad_all(card.root, 6, 0);
    lv_obj_set_style_pad_row(card.root, 4, 0);
    lv_obj_set_style_min_width(card.root, 0, 0);
    if (i < AMS2PRO_COUNT) {
      // Width and height are set in layoutOverviewBoard, by how many there are.
      lv_obj_set_flex_grow(card.root, 0);
      lv_obj_set_width(card.root, lv_pct(100));
      lv_obj_set_height(card.root, lv_pct(100));
    } else {
      // AMS-HT and external spools share one row, each as wide as the next.
      lv_obj_set_flex_grow(card.root, 1);
      lv_obj_set_height(card.root, lv_pct(100));
    }
    lv_obj_set_flex_flow(card.root, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(card.root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card.root, overviewCardEvent, LV_EVENT_CLICKED,
                        reinterpret_cast<void *>(static_cast<intptr_t>(i)));
    lv_obj_add_flag(card.root, LV_OBJ_FLAG_HIDDEN);

    card.head = lv_obj_create(card.root);
    lv_obj_remove_style_all(card.head);
    lv_obj_set_width(card.head, lv_pct(100));
    lv_obj_set_height(card.head, 18);
    lv_obj_set_flex_flow(card.head, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card.head, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                         LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card.head, 4, 0);
    lv_obj_clear_flag(card.head, LV_OBJ_FLAG_SCROLLABLE);

    card.label = lv_label_create(card.head);
    lv_obj_set_style_text_font(card.label, &lv_font_montserrat_ext_12, 0);
    lv_obj_set_style_text_color(card.label, lv_color_hex(COL_TEXT), 0);
    lv_label_set_long_mode(card.label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(card.label, 56);

    ensureClimateIcons();
    card.climateBox = lv_obj_create(card.head);
    lv_obj_remove_style_all(card.climateBox);
    lv_obj_set_height(card.climateBox, 16);
    lv_obj_set_flex_grow(card.climateBox, 1);
    lv_obj_set_flex_flow(card.climateBox, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(card.climateBox, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                         LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(card.climateBox, 3, 0);
    lv_obj_clear_flag(card.climateBox, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(card.climateBox, LV_OBJ_FLAG_CLICKABLE);

    card.tempIcon = makeClimateIcon(card.climateBox, &iconThermo);
    card.tempText = lv_label_create(card.climateBox);
    lv_obj_set_style_text_font(card.tempText, &lv_font_montserrat_ext_12, 0);
    lv_obj_set_style_text_color(card.tempText, lv_color_hex(COL_MUTED), 0);
    lv_obj_set_width(card.tempText, LV_SIZE_CONTENT);
    lv_label_set_text(card.tempText, "");

    card.humIcon = makeClimateIcon(card.climateBox, &iconDrop);
    lv_obj_set_style_pad_left(card.humIcon, 6, 0);
    card.humText = lv_label_create(card.climateBox);
    lv_obj_set_style_text_font(card.humText, &lv_font_montserrat_ext_12, 0);
    lv_obj_set_style_text_color(card.humText, lv_color_hex(COL_MUTED), 0);
    lv_obj_set_width(card.humText, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(card.humText, 2, 0);
    lv_label_set_text(card.humText, "");

    card.bayRow = lv_obj_create(card.root);
    lv_obj_remove_style_all(card.bayRow);
    lv_obj_set_width(card.bayRow, lv_pct(100));
    lv_obj_set_flex_grow(card.bayRow, 1);
    // AMS slots stay in one row. HT cards may wrap into a 2×2 when grouped.
    lv_obj_set_flex_flow(card.bayRow,
                         i < AMS2PRO_COUNT ? LV_FLEX_FLOW_ROW : LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_style_pad_row(card.bayRow, 4, 0);
    lv_obj_set_style_pad_column(card.bayRow, 4, 0);
    lv_obj_clear_flag(card.bayRow, LV_OBJ_FLAG_SCROLLABLE);

    for (int b = 0; b < MAX_OVERVIEW_SLOTS; ++b) {
      buildOverviewBay(card.bays[b], card.bayRow, i >= AMS2PRO_COUNT);
    }
    // Slots, labels, and bars sit on top of the card. Leave them non-clickable
    // so a tap anywhere on the unit opens that unit's page.
    passClicksToParent(card.root);
  }
}

}  // namespace amsboard
