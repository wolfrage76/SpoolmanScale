#include "services/board_image.h"

#include <string.h>

#include "boards/board.h"

namespace {

// One literal, and the scan compares against its first part instead of a
// second literal: a prefix of its own would be in every image as well, and
// the scan would find that one and read rubbish after it.
const char MARKER[] = "SpoolmanScale-board:" BOARD_ID "\n";
constexpr size_t PREFIX_LEN = sizeof("SpoolmanScale-board:") - 1;
constexpr size_t MARKER_LEN = sizeof(MARKER) - 1;
constexpr char   MARKER_END = '\n';

// The board that had no line to carry: every image from before this file.
constexpr bool UNMARKED_IS_OURS = (BOARD_VERSION_NODE[0] == '\0');

// Longer than any board name is allowed to become.
constexpr size_t NAME_MAX = 40;

BoardImageVerdict s_verdict = BOARD_IMAGE_UNMARKED;
size_t s_matched = 0;          // bytes of the prefix seen in a row
bool   s_in_name = false;      // the prefix is complete, the name follows
char   s_name[NAME_MAX + 1];
size_t s_name_len = 0;

void nameComplete() {
  const size_t own_len = MARKER_LEN - PREFIX_LEN - 1;
  const bool ours = (s_name_len == own_len) &&
                    (memcmp(s_name, MARKER + PREFIX_LEN, own_len) == 0);
  // One line for this board settles it; a foreign one only counts while none
  // for this board was seen.
  if (ours) s_verdict = BOARD_IMAGE_MATCH;
  else if (s_verdict == BOARD_IMAGE_UNMARKED) s_verdict = BOARD_IMAGE_FOREIGN;
}

}  // namespace

const char* boardImageMarker() {
  static char line[MARKER_LEN];
  memcpy(line, MARKER, MARKER_LEN - 1);
  line[MARKER_LEN - 1] = '\0';
  return line;
}

void boardImageScanBegin() {
  s_verdict  = BOARD_IMAGE_UNMARKED;
  s_matched  = 0;
  s_in_name  = false;
  s_name_len = 0;
}

BoardImageVerdict boardImageScanFeed(const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    const char c = (char)data[i];
    if (s_in_name) {
      if (c == MARKER_END) {
        nameComplete();
        s_in_name = false;
      } else if (s_name_len < NAME_MAX) {
        s_name[s_name_len++] = c;
      } else {
        s_in_name = false;     // not a board line after all
      }
      continue;
    }
    if (c == MARKER[s_matched]) {
      if (++s_matched == PREFIX_LEN) {
        s_matched  = 0;
        s_in_name  = true;
        s_name_len = 0;
      }
    } else {
      // A failed match starts over at this byte. That is enough: the line
      // stands by itself in the image, behind the end of another string.
      s_matched = (c == MARKER[0]) ? 1 : 0;
    }
  }
  return s_verdict;
}

bool boardImageScanAccepts() {
  if (s_verdict == BOARD_IMAGE_MATCH) return true;
  if (s_verdict == BOARD_IMAGE_FOREIGN) return false;
  return UNMARKED_IS_OURS;
}
