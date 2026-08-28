#ifndef REF_BUTTONS_H
#define REF_BUTTONS_H

#include <stdint.h>

// Debounced button reader with one-shot hold detection.
//
// Only holds matter to this sketch: A SHORT TAP IS NEVER AN ACTION on the
// play-clock screen, so that a button brushed against a sleeve mid-game
// cannot reset the play clock. That is the original design's central decision
// about buttons and it matters more on a watch worn during exercise, not less.
//
// Edges are caught by an interrupt handler, which records which way the pin
// went and when. poll() debounces from those timestamps, so a press that lands
// while the sketch is busy elsewhere is still measured from the real moment of
// contact rather than from whenever the sketch next got a chance to look.
// poll() also samples the pins directly, so a lost edge costs one poll
// interval rather than a button that has gone deaf.
//
// WHAT CHANGED IN THIS PORT. Three buttons instead of four, and each carries
// its own active level -- the OSW's SELECT reads LOW when pressed and its
// other two read HIGH (board.h). The debounce state machine is unaltered. Two
// things moved back towards the ESP32 the family started on: the edge
// handlers use attachInterruptArg() (this core has it, so the parent's four
// trampolines collapse to one function), and armSleepWake() is REAL again --
// light sleep on the ESP32 wakes on a GPIO LEVEL that has to be configured
// per pin, where the STM32's EXTI needed nothing at all.
//
// AND ONE THING WAS ADDED: releasedAfter(). The parent gave sleep its own
// button; here sleep and menu share the bottom-left one, split by hold
// length, so the main loop needs to know not just "held this long" but "was
// released after holding between this long and that long". One primitive
// covers the menu's tap-to-select as well.
namespace Buttons {

enum Id : uint8_t {
  LONG_TIMER = 0,  // top right
  SHORT_TIMER,     // bottom right
  SELECT,          // bottom left: menu / clear / sleep
  COUNT
};

// Sets the pins up, seeds the debounce state and attaches the edge handlers.
void begin();

// Re-seed the debounce state from the pins as they are right now, suppressing
// a hold (and any pending release) for any button already down. Used after
// waking, so a button still settling is not mistaken for a fresh press.
void resync();

// Sample every button. Call once per loop iteration, and from anywhere that is
// about to block for longer than a hold threshold.
void poll();

// True exactly once per press, the moment the button has been held for `ms`.
// Holding longer does not fire again; the button must be released first.
bool heldFor(Id id, uint32_t ms);

// True exactly once, on the poll after the button was released, if the press
// lasted at least `minMs` and less than `maxMs`. Consumed only when it
// matches, so two callers with disjoint windows can share a button.
bool releasedAfter(Id id, uint32_t minMs, uint32_t maxMs);

// True while the given button is down.
bool isDown(Id id);

// True while any button is down.
bool anyDown();

// Block until every button is released, or until BUTTON_RELEASE_TIMEOUT_MS
// elapses. Used before sleeping so a still-held button cannot immediately
// wake the watch back up.
void waitForRelease();

// Make the three buttons wake the chip from light sleep, and undo that after.
//
// ESP32 light sleep wakes on a configured GPIO LEVEL. armSleepWake() enables
// a level wake on every button at its own active level -- any press wakes the
// CORE, and main.cpp's sleep loop decides whether it wakes the WATCH. The
// caller must have released the buttons first (waitForRelease()), or the
// press that asked for sleep wakes us straight back.
void armSleepWake();
void disarmSleepWake();

} // namespace Buttons

#endif // REF_BUTTONS_H
