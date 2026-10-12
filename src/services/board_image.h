#pragma once

#include <stddef.h>
#include <stdint.h>

// ============================================================
//  WHICH BOARD AN IMAGE WAS BUILT FOR
//
//  An image starts on any ESP32-S3 and only works on the board it was built
//  for: on another one it looks for a display, a PSRAM and an expander that
//  are not there, and the device has to be flashed over USB to get back. The
//  update from GitHub cannot go wrong, every board downloads a file under its
//  own name. The upload in the browser takes whatever file it is handed.
//
//  So every image carries the name of its board as a line of text, and the
//  upload reads along for it. An image without the line is from before there
//  was a second board, and that one board still takes it.
// ============================================================

// The line this firmware carries, without its line end. For the boot log.
const char* boardImageMarker();

enum BoardImageVerdict {
  BOARD_IMAGE_UNMARKED,   // no line seen so far
  BOARD_IMAGE_MATCH,      // built for this board
  BOARD_IMAGE_FOREIGN,    // built for another board
};

// A new upload begins.
void boardImageScanBegin();
// The next bytes of the image, in order. The verdict as it stands after them:
// FOREIGN is final and the upload can be dropped on the spot.
BoardImageVerdict boardImageScanFeed(const uint8_t* data, size_t len);
// When the last byte is in: whether this board may start the image.
bool boardImageScanAccepts();
