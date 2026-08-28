#include "Buttons.h"

#include <Arduino.h>

#include "board.h"
#include "settings.h"

// The light-sleep wake plumbing is ESP-IDF; the host stub has no such thing
// and the host tests never sleep, so it is compiled out there.
#if defined(ESP32)
#include "driver/gpio.h"
#include "esp_sleep.h"
#endif

#ifndef ARDUINO_ISR_ATTR
#define ARDUINO_ISR_ATTR
#endif

namespace Buttons {
namespace {

struct Button {
  uint8_t pin;
  uint8_t active;   // digitalRead() level that means "pressed" -- per button,
                    // because the OSW mixes active-low and active-high

  // Written by the interrupt handler, read by poll() with interrupts masked.
  volatile bool     edgeLevel; // level the handler saw at the last edge
  volatile uint32_t edgeUs;    // when that edge landed
  volatile uint32_t edgeSeq;   // bumped on every edge, so a bounce that lands
                               // between two polls is not missed

  // poll() owns everything below.
  uint32_t seenSeq;    // last edge poll() has accounted for
  bool     raw;        // last level taken from the handler
  uint32_t rawSinceMs; // when that level settled
  bool     stable;     // debounced level: true == pressed
  uint32_t pressedAt;  // when `stable` last went true
  bool     holdFired;  // hold action already delivered for this press

  // The release report releasedAfter() reads. Written by poll() when `stable`
  // goes false; consumed by the first caller whose window matches.
  bool     releasePending;
  uint32_t releaseHeldMs; // how long that press had lasted
};

Button buttons[COUNT] = {
    {BTN_LONG_TIMER_PIN, BTN_LONG_TIMER_ACTIVE,
     false, 0, 0, 0, false, 0, false, 0, false, false, 0},
    {BTN_SHORT_TIMER_PIN, BTN_SHORT_TIMER_ACTIVE,
     false, 0, 0, 0, false, 0, false, 0, false, false, 0},
    {BTN_SELECT_PIN, BTN_SELECT_PIN_ACTIVE,
     false, 0, 0, 0, false, 0, false, 0, false, false, 0},
};

bool readPin(const Button &b) { return digitalRead(b.pin) == b.active; }

// Runs at interrupt time, so it touches nothing but this struct: no display,
// no I2C, no heap. Recording the edge is all it does; deciding what the edge
// means is poll()'s job. One handler for all three buttons, because this core
// has attachInterruptArg() -- the parent's per-button trampolines were only
// ever a workaround for a core that did not.
void ARDUINO_ISR_ATTR onEdge(void *arg) {
  Button &b = *(Button *)arg;
  b.edgeLevel = readPin(b);
  b.edgeUs = micros();
  b.edgeSeq = b.edgeSeq + 1;
}

} // namespace

void resync() {
  const uint32_t nowUs = micros();
  for (uint8_t i = 0; i < COUNT; i++) {
    Button &b = buttons[i];
    const bool level = readPin(b);

    noInterrupts();
    b.edgeLevel = level;
    b.edgeUs = nowUs;
    b.seenSeq = b.edgeSeq;
    interrupts();

    b.raw = level;
    b.stable = level;
    b.rawSinceMs = nowUs / 1000;
    b.pressedAt = b.rawSinceMs;
    // A button already down -- the one that just woke us, for instance -- must
    // not read as a fresh press, and a release the sleep loop already
    // swallowed must not surface as a stale report either.
    b.holdFired = level;
    b.releasePending = false;
  }
}

void begin() {
  for (uint8_t i = 0; i < COUNT; i++) {
    // Plain INPUT: every pull these buttons need is already on the OSW board
    // (or, for SELECT, on the module's BOOT strap). board.h says so once.
    pinMode(buttons[i].pin, BTN_MODE);
  }
  // Seed first, then arm: an edge arriving mid-seed would otherwise be thrown
  // away by the very code that is meant to catch it.
  resync();
  for (uint8_t i = 0; i < COUNT; i++) {
    attachInterruptArg(digitalPinToInterrupt(buttons[i].pin), onEdge,
                       &buttons[i], CHANGE);
  }
}

void poll() {
  for (uint8_t i = 0; i < COUNT; i++) {
    Button &b = buttons[i];

    // Take the handler's view of the pin, then check it against the pin
    // itself. An edge lost while interrupts were masked, or one that landed
    // during sleep, would otherwise leave the button stuck at its old level
    // for good. Sampling catches that, at the cost of a timestamp only as good
    // as this poll.
    noInterrupts();
    bool level = b.edgeLevel;
    if (readPin(b) != level) {
      level = !level;
      b.edgeLevel = level;
      b.edgeUs = micros();
      b.edgeSeq = b.edgeSeq + 1;
    }
    const uint32_t edgeUs = b.edgeUs;
    const uint32_t seq = b.edgeSeq;
    interrupts();

    // millis() is this same microsecond counter divided down, so the two stay
    // comparable, wrap included.
    const uint32_t edgeMs = edgeUs / 1000;
    const uint32_t now = millis();

    // Any edge at all restarts the debounce window, even one that leaves the
    // level where it was: a press followed by a bounce back and forth between
    // two polls has settled at the last edge, not the first.
    if (seq != b.seenSeq || level != b.raw) {
      b.seenSeq = seq;
      b.raw = level;
      b.rawSinceMs = edgeMs;
    }

    // The comparison is signed because a timestamp taken just after `now` is
    // legitimate -- the sampling path above reads the clock a moment later.
    if (b.raw != b.stable &&
        (int32_t)(now - b.rawSinceMs) >= (int32_t)BUTTON_DEBOUNCE_MS) {
      b.stable = b.raw;
      if (b.stable) {
        // Straight from the interrupt, so the hold is measured from the moment
        // of contact even when this is the first poll after a long stall.
        b.pressedAt = b.rawSinceMs;
        b.holdFired = false;
        b.releasePending = false;
      } else {
        // The press just ended; report how long it was. Measured to the
        // release EDGE rather than to this poll, for the same reason the
        // press is.
        b.releaseHeldMs = b.rawSinceMs - b.pressedAt;
        b.releasePending = true;
      }
    }
  }
}

bool heldFor(Id id, uint32_t ms) {
  Button &b = buttons[id];
  if (!b.stable || b.holdFired ||
      (int32_t)(millis() - b.pressedAt) < (int32_t)ms) {
    return false;
  }
  b.holdFired = true;
  return true;
}

bool releasedAfter(Id id, uint32_t minMs, uint32_t maxMs) {
  Button &b = buttons[id];
  if (!b.releasePending) {
    return false;
  }
  if (b.releaseHeldMs < minMs || b.releaseHeldMs >= maxMs) {
    return false;   // not consumed: another window may still want it
  }
  b.releasePending = false;
  return true;
}

bool isDown(Id id) { return buttons[id].stable; }

bool anyDown() {
  for (uint8_t i = 0; i < COUNT; i++) {
    if (buttons[i].stable) {
      return true;
    }
  }
  return false;
}

void waitForRelease() {
  const uint32_t deadline = millis() + BUTTON_RELEASE_TIMEOUT_MS;
  while ((int32_t)(millis() - deadline) < 0) {
    poll();
    if (!anyDown()) {
      return;
    }
    delay(BUTTON_POLL_MS);
  }
}

void armSleepWake() {
#if defined(ESP32)
  // A LEVEL wake on each button at its own active level: any press wakes the
  // core. Level rather than edge because that is what light sleep offers for
  // ordinary GPIOs -- and it is the right thing anyway, since a press that
  // lands a microsecond before the sleep instruction still wakes it.
  for (uint8_t i = 0; i < COUNT; i++) {
    gpio_wakeup_enable((gpio_num_t)buttons[i].pin,
                       buttons[i].active == HIGH ? GPIO_INTR_HIGH_LEVEL
                                                 : GPIO_INTR_LOW_LEVEL);
  }
  esp_sleep_enable_gpio_wakeup();
#endif
}

void disarmSleepWake() {
#if defined(ESP32)
  for (uint8_t i = 0; i < COUNT; i++) {
    gpio_wakeup_disable((gpio_num_t)buttons[i].pin);
  }
#endif
}

} // namespace Buttons
