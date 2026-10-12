#pragma once

#include <lvgl.h>

#include "ui/ams_board_model.h"

// The AMS View page of the CrowPanel AMS monitor, as a widget of this
// firmware: built into a parent, fed with units, and forgotten when the
// parent is deleted. See ams_board_view.cpp for where the code comes from.

namespace amsboard {

// Builds the page into `parent`, which is a column that gives it the height
// left under a header. `on_card` hears a tap on a unit. False when there is
// no memory for it.
bool viewBuild(lv_obj_t *parent, lv_event_cb_t on_card);

// After the parent was deleted: drops every pointer into it.
void viewForget();

// The line in the middle of the page while there is nothing to show.
void viewSetHint(const String &text);

// What the server said. Redraws what changed.
void viewSetUnits(const OverviewUnit *units, int count);

// A bay is too narrow for both figures, so they take turns.
void viewTogglePercentAndGrams();

// The theme colours the page uses, for the header that is built around it.
uint32_t viewBackgroundRgb();
uint32_t viewTextRgb();
uint32_t viewAccentRgb();
uint32_t viewMutedRgb();

}  // namespace amsboard
