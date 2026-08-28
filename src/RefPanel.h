#ifndef REF_PANEL_H
#define REF_PANEL_H

#include <Adafruit_GFX.h>
#include <stdint.h>

// The panel, behind the one seam the rest of the firmware draws through.
//
// WHAT CHANGED IN THE PORT. The parent's RefPanel wrapped a Sharp memory LCD:
// a reflective panel with its own pixel memory, a VCOM polarity that had to be
// inverted every second on pain of damage, and a DISP line that blanked it for
// free. This one wraps the Open-Smartwatch's GC9A01 -- an emissive round TFT
// with a backlight and no such obligations. What survives, deliberately, is
// the SHAPE of the parent's design:
//
//   THE FRAMEBUFFER LIVES IN THE MCU. A 1-bit GFX canvas (7.2 kB -- this
//   watch is black and white, per the README, so 16 bits per pixel would buy
//   nothing but heap pressure). Every screen draws into it and refresh()
//   pushes it whole. That is what makes a repaint flicker-free -- the panel
//   never sees a half-drawn frame -- and it is what RefMenu and RefDisplay
//   were written against.
//
//   ONE PUSH PATH. refresh() expands the canvas a row at a time into 16-bit
//   pixels and streams them inside one SPI transaction: about 115 kB on the
//   wire, ~35 ms at the OSW's 27 MHz. Fast enough that the parent's "a frame
//   is cheap, repaint the whole thing" simplification still holds.
//
//   blank()/unblank() ARE STILL THE WHOLE SLEEP DISPLAY. Here they are the
//   panel's own sleep-in/sleep-out commands plus the backlight -- a dark,
//   unlit panel IS the indication that the watch is asleep. The canvas keeps
//   its contents through light sleep, so waking could be a bare unblank and
//   refresh; main.cpp repaints anyway, because the wall clock has moved on.
//
// WHAT DID NOT SURVIVE: toggleVcom()/keepAlive(). There is no VCOM. A settled
// screen owes this panel nothing at all, and the keep-alive plumbing the
// parent threaded through every loop is simply gone.
namespace RefPanel {

// The two colours, as canvas values. THEME_FG/THEME_BG in RefDisplay pick
// which is which; refresh() maps 1 to white and 0 to black on the wire.
static const uint16_t PANEL_WHITE = 1;
static const uint16_t PANEL_BLACK = 0;

// The framebuffer every screen draws into. A full Adafruit_GFX, so the
// ported drawing code compiles against it unchanged.
extern GFXcanvas1 display;

// Bring up SPI, the controller and the backlight, and push one black frame so
// the panel never shows init noise.
void begin();

// Push the whole canvas to the panel.
void refresh();

// Backlight off, panel to sleep -- and back. The canvas is untouched.
void blank();
void unblank();

} // namespace RefPanel

#endif // REF_PANEL_H
