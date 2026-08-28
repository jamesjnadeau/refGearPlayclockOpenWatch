// The smallest Arduino a host compiler needs to read board.h and drive
// Buttons.cpp.
//
// NOT A SIMULATOR. It exists so the logic-heavy parts of this firmware can be
// compiled and tested on a laptop with no watch attached. Ported from the
// parent's stub with the STM32 pin-name table dropped -- the ESP32's pins are
// plain integers, so board.h's numbers are usable as-is -- and with
// attachInterruptArg() added, because that is the attach the ESP32 core has
// and the one Buttons.cpp uses.
//
// --- A FAKE CLOCK AND FAKE PINS ---------------------------------------------
//
// Time does not advance on its own. A test moves it, which is what makes a
// 30 ms debounce window and a 500 ms hold exactly reproducible instead of
// approximately so.

#pragma once

#include <cstdint>
#include <cstring>

#define INPUT         0x01
#define OUTPUT        0x03
#define INPUT_PULLUP  0x05

#define LOW  0x0
#define HIGH 0x1

#define CHANGE 0x03

inline unsigned long __fake_us = 0;
inline int __fake_level[64] = {};
inline void (*__fake_isr[64])(void *) = {};
inline void *__fake_isr_arg[64] = {};

inline unsigned long micros() { return __fake_us; }
inline unsigned long millis() { return __fake_us / 1000UL; }

inline void pinMode(uint8_t, int) {}
inline int digitalRead(uint8_t pin) { return __fake_level[pin]; }
inline void digitalWrite(uint8_t pin, int v) { __fake_level[pin] = v; }
inline int digitalPinToInterrupt(uint8_t pin) { return pin; }
inline void attachInterruptArg(int pin, void (*fn)(void *), void *arg, int) {
    __fake_isr[pin] = fn;
    __fake_isr_arg[pin] = arg;
}
inline void noInterrupts() {}
inline void interrupts() {}

// --- what a test drives -----------------------------------------------------

// Advance the clock. Nothing else happens: no ISR fires, no poll runs.
inline void fakeAdvanceMs(unsigned long ms) { __fake_us += ms * 1000UL; }

// Move a pin and fire its edge handler, exactly as the hardware would. The
// handler runs BEFORE the test regains control, which is the ordering that
// matters -- Buttons is built around the edge being timestamped at the moment
// of contact rather than at the next poll.
inline void fakeSetPin(uint8_t pin, int level) {
    __fake_level[pin] = level;
    if (__fake_isr[pin]) {
        __fake_isr[pin](__fake_isr_arg[pin]);
    }
}

// Move a pin WITHOUT firing its handler: an edge lost while interrupts were
// masked, or one that landed during sleep. Buttons is supposed to recover from
// this on the next poll, and that recovery is worth testing because it is the
// difference between one missed poll and a button that has gone deaf.
inline void fakeSetPinSilently(uint8_t pin, int level) {
    __fake_level[pin] = level;
}

inline void fakeReset() {
    __fake_us = 0;
    std::memset(__fake_level, 0, sizeof(__fake_level));
    std::memset(__fake_isr, 0, sizeof(__fake_isr));
    std::memset(__fake_isr_arg, 0, sizeof(__fake_isr_arg));
}

inline void delay(unsigned long ms) { fakeAdvanceMs(ms); }
