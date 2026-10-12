#pragma once

// ============================================================
//  AMS BOARD
//
//  The whole AMS of a printer on one page, drawn for a panel with more
//  pixels than the UI, and kept up to date for as long as it stands. It is
//  the AMS View page of the CrowPanel AMS monitor: that project's layout,
//  palette and swatches, taken over as they are (ams_board_view.cpp), and fed
//  from FilaMan's display answer the way that project reads it
//  (services/ams_board_feed.cpp).
//
//  It exists only on a board with BOARD_NATIVE_W and _H, and only with
//  FilaMan as the backend: no other describes a spool with several colours, a
//  finish and a maker. There the AMS chip in the header opens it, and the AMS
//  view every other board goes to is one tap further: a unit, or the list
//  symbol in the header. Everything that asks or writes - a bay's card, an
//  assignment - stays in that view, which is why this page can be the one
//  screen that is not 480x320.
//
//  That is also the one rule it lives by. Nothing else in the firmware can
//  be shown at the panel's own resolution, so the moment anything else comes
//  up, a popup included, the page takes itself down and hands the pixels
//  back first.
// ============================================================

// Whether this board has the panel for it.
bool amsBoardAvailable();

// Opens the page. Only raises a flag: the display is switched over and the
// server asked from the loop, never from the tap.
void requestAmsBoard();

// Runs the parked work: open, fetch, redraw, close. Called from appLoop().
void handleAmsBoardDeferredActions();

bool isAmsBoardOpen();

// Takes the page down and gives the display back to the 480x320 UI. For
// hideAllOverlays() and showMainScreen(): every way out of it goes through
// one of the two.
void destroyAmsBoard();
