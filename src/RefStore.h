#ifndef REF_STORE_H
#define REF_STORE_H

#include <stdint.h>

// The handful of settings that have to survive a reboot.
//
// BACK ON NVS, WHERE THIS FAMILY STARTED. The original watchy-ref-counter
// used the ESP32's Preferences (NVS); the STM32 rebuild had no NVS and built
// this class over emulated flash, with RAM buffering because every commit
// there was a full page erase. This port is on an ESP32 again, so Preferences
// is back underneath -- with its wear levelling, the buffering is no longer
// load-bearing -- BUT THE INTERFACE IS KEPT: set() touches RAM, commit()
// writes, and nothing else in the firmware talks to storage directly. Two
// reasons rather than sentiment:
//
//   EVERY CALLER PORTS UNCHANGED. RefSport and the menu were written against
//   set/commit, and "change five numbers, write once" is still the right
//   shape even when writes are cheap.
//
//   THE PER-KEY "WRITTEN" SEMANTICS ARE THE CONTRACT. get() returns the
//   fallback unless this key has actually been stored -- which is what lets a
//   store holding only a sport index leave every Custom field on its
//   settings.h default. NVS gives us that per-key existence natively.
namespace RefStore {

enum Key : uint8_t {
  KEY_SPORT = 0,      // selected preset index
  KEY_CUSTOM_LONG,    // the Custom preset's five numbers
  KEY_CUSTOM_SHORT,
  KEY_CUSTOM_WARN,
  KEY_CUSTOM_WARN2,
  KEY_CUSTOM_FINAL,
  KEY_CLOCK_SET_AT,   // epoch of the last hand-set, for RefDrift
  KEY_COUNT,
};

// Open the store and read what it holds. Call once at startup, before
// anything asks for a value.
void begin();

// True when begin() found at least one stored value. False means every get()
// will hand back its fallback, which is the correct behaviour on a fresh
// board.
bool loaded();

// Returns `fallback` unless this key has actually been written.
uint32_t get(Key k, uint32_t fallback);

// True when this key has been written at least once.
bool has(Key k);

// RAM only. Nothing reaches flash until commit().
void set(Key k, uint32_t value);

// Write every changed key to NVS. A commit with nothing changed writes
// nothing.
void commit();

// The common case: change one setting and save it.
void setAndCommit(Key k, uint32_t value);

// Forget everything, in RAM and in NVS. This is what a factory reset row in
// the menu would call, and it is what the tests use to start from a board
// that has never been written.
void clear();

// How many commits have actually reached flash since boot. Exposed so the
// write pattern is measurable rather than assumed.
uint32_t writeCount();

} // namespace RefStore

#endif // REF_STORE_H
