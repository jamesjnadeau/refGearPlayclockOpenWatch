#ifndef REF_PLAY_CLOCK_H
#define REF_PLAY_CLOCK_H

#include <stdint.h>

#include "RefDisplay.h"   // AppState
#include "RefSport.h"

// The play clock itself: what state the watch is in, how many seconds are
// left, and what the motor should say about the second that just passed.
//
// LIFTED OUT OF THE SKETCH, WHICH IS WHERE THE PARENT KEPT IT. RefCounter.ino
// held `state`, `durationSec`, `shownSec`, `startUs` and buzzForMark() as file
// statics, so none of it could be exercised without an ESP32. It is arithmetic
// and a four-state machine; both belong somewhere a host can reach them.
//
// TWO THINGS THIS CLASS EXISTS TO GET RIGHT.
//
// THE REMAINING TIME IS RECOMPUTED, NEVER ACCUMULATED. Every tick derives the
// answer from the moment the clock started, so a buzz that blocks or a frame
// that runs long costs nothing in accuracy -- it can drop a redraw, but it
// cannot make the clock slow. This is the parent's design and it is the whole
// reason start() takes a timestamp rather than reading one.
//
// THE PRESET IS CAPTURED AT start(), NOT READ WHILE RUNNING. The parent called
// RefSport::active() from inside buzzForMark on every tick, so changing the
// sport mid-countdown would have moved the marks under a clock already
// running. Nothing in the parent's UI could do that -- the menu is unreachable
// while a clock runs -- which is exactly why it was never noticed. Copying the
// five numbers in costs ten bytes and removes the question.
//
// NO ARDUINO AND NO PANEL. millis() is the caller's to supply; this class only
// ever compares timestamps, so it is wrap-safe by construction.

// What the motor should say about a particular second.
enum Mark : uint8_t {
  MARK_NONE = 0,
  MARK_TICK,    // inside the final countdown: one click per second
  MARK_WARN,    // an early warning mark
  MARK_EXPIRE,  // zero
};

struct MarkPlan {
  Mark    mark;
  uint8_t count;   // how many repeats; 1 for everything but the warnings
};

class PlayClock {
public:
  // Back to the ready screen with nothing on the clock.
  void reset();

  // Start (or restart) a countdown of `seconds` under `p`, as of `nowMs`.
  void start(const RefSport::Preset &p, uint16_t seconds, uint32_t nowMs);

  // Advance to `nowMs`. Returns true when the displayed second changed, which
  // is the only time the caller has to redraw or to buzz. mark() then says
  // what that second earned.
  bool tick(uint32_t nowMs);

  // True once the EXPIRED screen has been up long enough to give way. Always
  // false in any other state, so a caller can ask unconditionally.
  bool expiredHoldDone(uint32_t nowMs) const;

  AppState state() const { return _state; }
  uint16_t secondsLeft() const { return _shown; }
  uint16_t durationSec() const { return _duration; }

  // What the last tick() that returned true landed on. MARK_NONE otherwise.
  MarkPlan mark() const { return _mark; }

  // Which buzz a given second deserves under `p`.
  //
  // THE ORDER IS THE BEHAVIOUR. The per-second countdown takes precedence over
  // both warnings, so a warning mark set at or below finalCountdownFrom is
  // SWALLOWED -- the second buzzes anyway, as a tick rather than as a warning.
  // The parent documented that in settings.h and never tested it; it is the
  // first thing playclock_test checks.
  static MarkPlan markFor(const RefSport::Preset &p, uint16_t secondsLeft);

private:
  AppState _state    = STATE_IDLE;
  uint16_t _duration = 0;
  uint16_t _shown    = 0;

  // Wall-clock origin of the running countdown, and when the EXPIRED screen
  // may give way. Both are compared as differences, never as magnitudes.
  uint32_t _startMs   = 0;
  uint32_t _expiredAt = 0;

  RefSport::Preset _preset = {};
  MarkPlan         _mark   = {MARK_NONE, 0};
};

#endif // REF_PLAY_CLOCK_H
