#ifndef REF_MENU_H
#define REF_MENU_H

#include "RefRtc.h"

// The settings menu, opened with the bottom-left button from the ready
// screen. Runs modally and returns once the user backs out or it times out;
// the caller repaints its own screen afterwards.
//
// FOUR ROWS, WHERE THE PARENT HAD NINE. About, Set time, Sport, Edit Custom.
// The five that went are RefMenuItems' story.
//
// TAKES THE RTC, WHERE THE PARENT TOOK A RefClock. That class wrapped the RTC
// together with NTP, BLE, the zone rules and the resync schedule -- four of
// which do not exist here. What the menu actually needs is the chip: read the
// time to seed the set-time screen, write it back, and read it again for the
// About screen. Everything else it needs it can reach directly.
namespace RefMenu {

void open(RefRtc &rtc);

} // namespace RefMenu

#endif // REF_MENU_H
