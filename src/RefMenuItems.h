#ifndef REF_MENU_ITEMS_H
#define REF_MENU_ITEMS_H

#include <stddef.h>
#include <stdint.h>

// The settings menu's row list: which rows exist, what order they come in, and
// what each one says.
//
// Split out of the menu's drawing code so it can be compiled and tested on a
// host, which is the seam the parent had and which is worth keeping.
//
// FIVE OF THE PARENT'S NINE ROWS ARE GONE, and with them most of this file.
// ITEM_SYNC and ITEM_SYNC_BT synced the clock over NTP and Bluetooth;
// ITEM_WIFI configured the radio. There is no radio (spec D3). ITEM_ZONE and
// ITEM_DST belonged to RefZone, which is not ported -- with no network there
// is no UTC to offset, so the user sets local time directly and shiftHour()
// covers daylight saving. See Task 24 Step 7.
//
// AND buildVisible() AND slotOf() ARE GONE WITH THEM. They existed because two
// rows were conditional: "Sync NTP" was hidden until Wi-Fi credentials were
// saved, so running "Setup WiFi" could make it appear ABOVE the row you were
// standing on and move everything down one. Every row here is unconditional,
// so there is no rebuild to survive and no slot to track. The plan calls this
// moot and asks for the comment to go with the behaviour; both are gone.
namespace RefMenu {

// Menu order. About first, because that is the row the menu opens on.
//
// SET TIME IS SECOND, DELIBERATELY. It is the most important row in this menu
// and the least often used -- about once a year at +/-1 ppm, and again after
// every full discharge, because the RTC has no backup supply. Someone who has
// not touched it in a year has to find it without hunting, and the top of the
// list is where they will look. The sport rows are last: they are set once for
// the season, where everything above them is either read or reached for in a
// hurry.
enum Item : uint8_t {
  ITEM_ABOUT,
  ITEM_SET_TIME,
  ITEM_SPORT,
  ITEM_EDIT_CUSTOM,
  ITEM_COUNT,
};

// The widest label is "Edit Custom" at 11 glyphs and the widest value is
// "Base NCAA" at 9. 24 leaves room rather than sizing the buffer to the exact
// width and hoping nothing grows past it. What actually FITS across the panel
// is settings.h's MENU_LABEL_MAX_CHARS, which is a smaller number and a
// measured one.
static const size_t ITEM_LABEL_MAX = 24;

// The row's fixed text, and the live value drawn beside it.
//
// *** THE PARENT HAD ONE FUNCTION HERE AND THIS PANEL CANNOT DRAW ITS OUTPUT.
// *** itemLabel(ITEM_SPORT) returned "Sport: Base NCAA", which is 176 px in
// the parent's menu font on a 200 px panel. This one is 128 px wide, and that
// string does not fit in ANY font Adafruit_GFX ships -- 149 px in FreeSans9pt,
// 139 px in the narrowest 9 pt face there is, and 9 pt is the smallest size in
// the library. There is no font that rescues it.
//
// So the row splits: a short label on the left and its value right-aligned on
// the right, drawn smaller. That keeps the property the parent's single string
// was there for -- THE MENU DOUBLES AS THE STATUS DISPLAY, and which sport is
// loaded is the thing worth checking before a game -- while fitting a panel
// two thirds the width. It also means the sport is on screen from the moment
// the menu opens, rather than only when its row is highlighted, which a
// detail line under the list would not have given.
//
// itemValue() writes an empty string for every row that has no value. Rows
// with nothing to say are the normal case, not an error.
void itemLabel(uint8_t item, char *buf, size_t n);
void itemValue(uint8_t item, char *buf, size_t n);

} // namespace RefMenu

#endif // REF_MENU_ITEMS_H
