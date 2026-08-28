#ifndef REF_BUZZER_H
#define REF_BUZZER_H

#include <stdint.h>

// The vibration motor -- when there is one.
//
// THE INTERFACE IS THE PARENT'S AND THE HARDWARE IS ITS GRANDPARENT'S. The
// STM32 build drove a DRV2605L playing named ROM effects; the OSW Light has
// no haptic driver and NO MOTOR AT ALL (board.h's PIN_VIB), and OSW editions
// that do have one drive it from a bare GPIO -- which is exactly what the
// original watchy-ref-counter drove. So the named-effect interface survives,
// the ROM library does not, and each effect is realised as timed pulses on
// the pin, with the durations back in settings.h where that firmware kept
// them.
//
// THE PATTERN PLAYS IN THE BACKGROUND, exactly as it did on both ancestors:
// an esp_timer callback walks the pattern while loop() carries on, so a buzz
// never delays a frame and the buttons stay live throughout. On the no-motor
// build every function below compiles to a no-op and begin() returns false,
// which main.cpp reports once at boot.
namespace Buzzer {

// What the watch has to say. The durations are in settings.h.
enum Effect : uint8_t {
  TICK = 0,   // a play-clock tick: one short pulse
  WARN,       // the warning mark: a double pulse
  EXPIRE,     // zero: one long buzz
};

// Sets the pin up. Returns false when this edition has no motor.
bool begin();

// Play one effect, once.
void play(Effect e);

// Play `count` repeats of an effect, `gapMs` apart.
void play(Effect e, uint8_t count, uint32_t gapMs);

// True while a pattern is still playing.
bool busy();

// Stop immediately and drop anything queued. CALLED BEFORE EVERY SLEEP: a
// buzz left running across a sleep is a motor running until the battery
// gives out.
void off();

} // namespace Buzzer

#endif // REF_BUZZER_H
