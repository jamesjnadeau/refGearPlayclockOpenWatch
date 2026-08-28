#ifndef REF_LOG_H
#define REF_LOG_H

// Serial diagnostics.
//
// SIMPLER THAN THE PARENT'S, BECAUSE THE DANGER IS GONE. There, Serial with
// the USB console compiled out fell back to a UART whose default pins were a
// button and the panel's chip select, so a stray Serial call could drive the
// hardware; the macro existed to make that impossible. On the ESP32 the
// default Serial is UART0 on the module's own TX/RX pads, which the OSW
// exposes only on its edge connector -- nothing on the watch is disturbed by
// using it, and it is how the board is flashed anyway. So logging is simply
// on, always, at the cost of a few bytes of flash.
//
// LOG(...) keeps the parent's printf style so every ported call site compiles
// unchanged.

#include <Arduino.h>
#include <stdio.h>

#define LOG_BEGIN()                                                            \
  do {                                                                         \
    Serial.begin(115200);                                                      \
  } while (0)

#define LOG(...)                                                               \
  do {                                                                         \
    char _b[128];                                                              \
    snprintf(_b, sizeof(_b), __VA_ARGS__);                                     \
    Serial.println(_b);                                                        \
  } while (0)

#endif // REF_LOG_H
