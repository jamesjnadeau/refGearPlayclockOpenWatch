#include "Buzzer.h"

#include <Arduino.h>

#include "board.h"
#include "settings.h"

#if PIN_VIB

#include "esp_timer.h"

namespace Buzzer {
namespace {

// A pattern is a flat list of steps, each "pin on/off for this long". Built
// whole by play() and then walked by a one-shot esp_timer chain, so the whole
// thing runs behind loop()'s back. 32 steps holds the worst request the
// firmware makes (WARNING_BUZZ_COUNT_2 double pulses with gaps) several times
// over; a request that would not fit is truncated at the end, never mid-buzz.
struct Step {
  bool     on;
  uint32_t ms;
};

const uint8_t MAX_STEPS = 32;

Step    steps[MAX_STEPS];
uint8_t stepCount = 0;
volatile uint8_t stepNext = 0;   // next step to start; == stepCount when idle
esp_timer_handle_t timer = nullptr;

void motor(bool on) { digitalWrite(PIN_VIB, on ? VIB_ACTIVE : !VIB_ACTIVE); }

void onTimer(void *);

void startStep() {
  if (stepNext >= stepCount) {
    motor(false);
    return;
  }
  const Step &s = steps[stepNext];
  stepNext++;
  motor(s.on);
  esp_timer_start_once(timer, (uint64_t)s.ms * 1000ULL);
}

void onTimer(void *) { startStep(); }

void addStep(bool on, uint32_t ms) {
  if (stepCount < MAX_STEPS && ms > 0) {
    steps[stepCount++] = {on, ms};
  }
}

// One unit of an effect: what a single repeat feels like.
void addUnit(Effect e) {
  switch (e) {
  case TICK:
    addStep(true, BUZZ_TICK_MS);
    break;
  case WARN:
    // A double pulse, which is what the DRV2605L's "double click" was.
    addStep(true, BUZZ_WARN_MS);
    addStep(false, BUZZ_WARN_MS);
    addStep(true, BUZZ_WARN_MS);
    break;
  case EXPIRE:
    addStep(true, BUZZ_EXPIRE_MS);
    break;
  }
}

} // namespace

bool begin() {
  pinMode(PIN_VIB, OUTPUT);
  motor(false);
  if (timer == nullptr) {
    const esp_timer_create_args_t args = {
        .callback = onTimer,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "buzzer",
        .skip_unhandled_events = true,
    };
    if (esp_timer_create(&args, &timer) != ESP_OK) {
      timer = nullptr;
      return false;
    }
  }
  return true;
}

void play(Effect e, uint8_t count, uint32_t gapMs) {
  if (timer == nullptr) {
    return;
  }
  off();
  for (uint8_t i = 0; i < count; i++) {
    addUnit(e);
    if (i + 1 < count) {
      addStep(false, gapMs);
    }
  }
  startStep();
}

void play(Effect e) { play(e, 1, HAPTIC_GAP_MS); }

bool busy() { return stepNext < stepCount; }

void off() {
  if (timer != nullptr) {
    esp_timer_stop(timer);
  }
  stepCount = 0;
  stepNext = 0;
  motor(false);
}

} // namespace Buzzer

#else // no motor on this edition

namespace Buzzer {

bool begin() { return false; }
void play(Effect) {}
void play(Effect, uint8_t, uint32_t) {}
bool busy() { return false; }
void off() {}

} // namespace Buzzer

#endif
