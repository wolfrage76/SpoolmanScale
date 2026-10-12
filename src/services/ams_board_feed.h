#pragma once

#include <stdint.h>

#include "ui/ams_board_model.h"

// Where the AMS board gets what it draws: FilaMan's display answer for one
// printer, read into the board's own model. That model carries what
// AmsSlotState has no room for - the several colours of a silk or gradient
// spool, its finish, its maker - which is why the board asks for itself
// instead of drawing what the AMS view already has.
//
// FilaMan only. No other backend describes a spool this way.

namespace amsboard {

// Whether the active backend can feed the board.
bool feedAvailable();

// One request, blocking for as long as the server takes. 200 and the units
// in *units (a table the feed keeps, valid until the next call) with their
// number in *count, or the HTTP code, or a negative number for an answer
// that did not parse.
int feedFetch(int printer_id, const OverviewUnit **units, int *count,
              PrinterLine *line, uint32_t timeout_ms);

}  // namespace amsboard
