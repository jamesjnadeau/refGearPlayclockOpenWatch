#ifndef REF_DISPLAY_H
#define REF_DISPLAY_H

#include <stdint.h>

// What the watch is currently doing.
enum AppState : uint8_t {
  STATE_IDLE,     // no clock running, waiting for a button hold
  STATE_RUNNING,  // counting down
  STATE_EXPIRED,  // reached zero, showing 00 until the next clock is started
};

// THERE IS NO STATE_SLEEPING, exactly as in the parent. A sleeping watch
// blanks the panel and lights nothing -- see RefPanel::blank() -- so there is
// nothing for a sleeping state to draw and nothing to describe. A state the
// machine can never be seen in is a state that misleads.

// Everything the screen needs to draw itself. Passed in rather than read from
// the application layer, so the display can be exercised without it.
struct View {
  AppState state;
  uint16_t secondsLeft;   // RUNNING / EXPIRED only
  uint16_t durationSec;   // which clock is (or was) running
  uint16_t idleLongSec;   // IDLE only: the upper row
  uint16_t idleShortSec;  // IDLE only: the lower row
  uint8_t  hour;          // wall clock from the RTC, shown in the header
  uint8_t  minute;
  bool     clockValid;    // false renders --:-- instead
};

// Theme colours, derived from DARK_MODE. Declared here so the settings menu
// paints in the same scheme as the play clock. On this port that scheme is
// white-on-black by requirement -- see settings.h.
extern const uint16_t THEME_FG;
extern const uint16_t THEME_BG;

namespace RefDisplay {

void begin();

// Redraw the whole screen and push it to the panel. ONE ENTRY POINT: a whole
// frame is a ~35 ms canvas push, so there is no partial-window machinery and
// no reason for any.
void render(const View &v);

// Draw the whole screen into the framebuffer without pushing it. render() is
// this plus a refresh; it is exposed separately so a caller that wants to
// batch several changes into one frame can.
void paint(const View &v);

// Push the framebuffer to the panel. Use this when something changed.
//
// AND ONLY WHEN SOMETHING CHANGED. The parent's panel also demanded a VCOM
// keep-alive on a schedule; the GC9A01 demands nothing, so the keepAlive()
// half of this interface is gone and a settled screen costs zero SPI traffic.
void refresh();

// The cell voltage behind the header's gauge.
//
// EXPOSED FOR THE ABOUT SCREEN, which is the one place a number is more use
// than a bar -- on this board doubly so, because the OSW's divider has never
// been measured by this project and the shipped OSW firmware itself does not
// trust it into volts. See board.h's BATT_DIVIDER note.
float batteryVolts();

// Blank the panel without losing the framebuffer, and bring it back. The
// whole of the sleep display; RefPanel owns the mechanics.
void blank();
void unblank();

// A zero-padded two-digit value in the same seven-segment face as the
// countdown, at a size suited to the menu's set-time screen. `lit` false draws
// it in the background colour, which is how that screen blinks a field.
void drawDigitPair(int16_t x, int16_t y, uint8_t value, bool lit);
extern const int16_t DIGIT_PAIR_W;
extern const int16_t DIGIT_PAIR_H;

} // namespace RefDisplay

#endif // REF_DISPLAY_H
