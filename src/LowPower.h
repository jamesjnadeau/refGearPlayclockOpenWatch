#ifndef REF_LOW_POWER_H
#define REF_LOW_POWER_H

#include <stdint.h>

// Light sleep, which is where this watch is supposed to spend its life.
//
// WHY LIGHT SLEEP AND NOT DEEP SLEEP. This family of firmwares has been on
// both sides of that line. The original watchy-ref-counter used ESP32 DEEP
// sleep -- the chip REBOOTS on wake, so a third of its sketch was machinery
// to work out why it had woken and reconstruct its state. The STM32 rebuild
// used Stop2, which retains SRAM, and deleted all of it: a conventional
// loop() that sleeps between events. THIS PORT IS BACK ON AN ESP32 AND KEEPS
// THE STM32'S SHAPE ANYWAY, because the ESP32 also has a sleep that retains
// SRAM: light sleep. esp_light_sleep_start() returns into the middle of the
// calling function with every variable, the play clock and the framebuffer
// intact -- no reset, no wake-cause, nothing to reconstruct.
//
// What it costs: light sleep holds the chip near 0.8 mA where deep sleep is
// microamps. On this platform that trade is barely a choice anyway -- the
// bottom-right button is GPIO 10, which is not an RTC-domain pin, so DEEP
// sleep could never wake on it; light sleep's GPIO wake works on every pin.
// A prototype that keeps the parent's semantics beats a firmware that
// reintroduces the reconstruct-on-boot machinery to save a milliamp.
//
// WHAT SURVIVES: all of SRAM, so every variable, the play clock's state, and
// the 7.2 kB canvas. Peripheral configuration. GPIO output levels.
//
// AND -- UNLIKE THE STM32 -- millis() SURVIVES TOO: the ESP32 compensates its
// timer from the RTC slow clock on wake, so time appears to have passed. The
// firmware does not rely on that (the parent's rule that millis() is never a
// wall clock is kept, and the cached time is thrown away on wake regardless),
// but it means nothing here has to repair the tick either.
namespace LowPower {

// Enter light sleep and return when an interrupt wakes the core.
//
// THE CALLER OWNS EVERYTHING ELSE. This function does not blank the panel,
// stop the motor or release the buttons, because it cannot know what the
// caller wanted left running. main.cpp's enterSleep() is the one that does
// all of it, in the order that matters. The wake source is whatever
// Buttons::armSleepWake() configured.
void stopUntilInterrupt();

// One-time setup: drop the CPU to a frequency that still runs everything
// comfortably. Called once at startup.
void begin();

} // namespace LowPower

#endif // REF_LOW_POWER_H
