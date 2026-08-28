#include "RefMenuItems.h"

#include <stdio.h>
#include <string.h>

#include "RefSport.h"

namespace RefMenu {

void itemLabel(uint8_t item, char *buf, size_t n) {
  if (buf == nullptr || n == 0) {
    return;
  }
  switch (item) {
  case ITEM_ABOUT:
    snprintf(buf, n, "About");
    break;
  case ITEM_SET_TIME:
    snprintf(buf, n, "Set time");
    break;
  case ITEM_SPORT:
    // The sport itself is itemValue()'s, not this one's -- see the header for
    // why the parent's combined string cannot be drawn on a 128 px panel.
    snprintf(buf, n, "Sport");
    break;
  case ITEM_EDIT_CUSTOM:
    snprintf(buf, n, "Edit Custom");
    break;
  default:
    buf[0] = '\0';
    break;
  }
}

void itemValue(uint8_t item, char *buf, size_t n) {
  if (buf == nullptr || n == 0) {
    return;
  }
  switch (item) {
  case ITEM_SPORT:
    // The live value, read every time the menu paints. This is the half of
    // the parent's "Sport: Base NCAA" that made the menu a status display.
    snprintf(buf, n, "%s", RefSport::active().name);
    break;
  default:
    buf[0] = '\0';
    break;
  }
}

} // namespace RefMenu
