#include "RefSport.h"

#include <string.h>

#include "RefStore.h"
#include "settings.h"

namespace RefSport {
namespace {

// The fixed presets, in the order the picker lists them. Values follow the
// ReadyRef model line-up.
//
// Football reproduces what the parent firmware shipped with, so a watch that
// has never opened the menu behaves exactly as it did before presets existed.
const Preset FIXED[] = {
    {"Football",  "football",       40, 25, 10,  0, 5},
    {"Lacrosse",  "lacrosse shot", 120, 20, 30, 10, 5},
    {"Base NCAA", "NCAA baseball", 120, 20, 30, 10, 5},
    {"Base NFHS", "NFHS baseball",  80, 20, 30, 10, 5},
    {"Soft NCAA", "NCAA softball",  90, 20, 30, 10, 5},
    {"Soft NFHS", "NFHS softball",  60, 20, 20, 10, 5},
};
const uint8_t FIXED_COUNT  = sizeof(FIXED) / sizeof(FIXED[0]);
const uint8_t CUSTOM_INDEX = FIXED_COUNT;
const uint8_t PRESET_COUNT = FIXED_COUNT + 1;

uint8_t selected = 0;

// The Custom slot. Its two strings never change; the numbers are replaced from
// settings.h in begin() and from the store or the editor after that.
Preset customPreset = {"Custom", "user set", 40, 25, 10, 0, 5};

uint8_t clampIndex(uint8_t i) { return i < PRESET_COUNT ? i : 0; }

uint16_t clampClock(uint16_t v) {
  if (v < MIN_CLOCK_SECONDS) {
    return MIN_CLOCK_SECONDS;
  }
  return v > MAX_SECONDS ? MAX_SECONDS : v;
}

// Marks may be 0, which turns them off.
uint16_t clampMark(uint16_t v) { return v > MAX_SECONDS ? MAX_SECONDS : v; }

} // namespace

uint8_t count() { return PRESET_COUNT; }

bool isCustom(uint8_t i) { return clampIndex(i) == CUSTOM_INDEX; }

Preset preset(uint8_t i) {
  i = clampIndex(i);
  return i == CUSTOM_INDEX ? customPreset : FIXED[i];
}

void begin() {
  // Resolve the compiled-in default first, so a watch with nothing stored yet
  // still comes up on a sensible pair of clocks.
  selected = 0;
  if (strcmp(customPreset.name, DEFAULT_SPORT) == 0) {
    selected = CUSTOM_INDEX;
  } else {
    for (uint8_t i = 0; i < FIXED_COUNT; i++) {
      if (strcmp(FIXED[i].name, DEFAULT_SPORT) == 0) {
        selected = i;
        break;
      }
    }
  }

  customPreset.longSeconds = clampClock(CUSTOM_LONG_SECONDS);
  customPreset.shortSeconds = clampClock(CUSTOM_SHORT_SECONDS);
  customPreset.warnAtSeconds = clampMark(CUSTOM_WARN_SECONDS);
  customPreset.warn2AtSeconds = clampMark(CUSTOM_WARN2_SECONDS);
  customPreset.finalCountdownFrom = clampMark(CUSTOM_FINAL_FROM);

  // Each of these falls back to what was just resolved, and RefStore's
  // per-key written bitmap is what makes that work: a store holding only a
  // sport index leaves every Custom field on its settings.h value rather than
  // reading it back as zero.
  selected = clampIndex((uint8_t)RefStore::get(RefStore::KEY_SPORT, selected));
  customPreset.longSeconds = clampClock(
      (uint16_t)RefStore::get(RefStore::KEY_CUSTOM_LONG,
                              customPreset.longSeconds));
  customPreset.shortSeconds = clampClock(
      (uint16_t)RefStore::get(RefStore::KEY_CUSTOM_SHORT,
                              customPreset.shortSeconds));
  customPreset.warnAtSeconds = clampMark(
      (uint16_t)RefStore::get(RefStore::KEY_CUSTOM_WARN,
                              customPreset.warnAtSeconds));
  customPreset.warn2AtSeconds = clampMark(
      (uint16_t)RefStore::get(RefStore::KEY_CUSTOM_WARN2,
                              customPreset.warn2AtSeconds));
  customPreset.finalCountdownFrom = clampMark(
      (uint16_t)RefStore::get(RefStore::KEY_CUSTOM_FINAL,
                              customPreset.finalCountdownFrom));
}

uint8_t index() { return selected; }

void setIndex(uint8_t i) {
  selected = clampIndex(i);
  RefStore::setAndCommit(RefStore::KEY_SPORT, selected);
}

Preset active() { return preset(selected); }

Preset custom() { return customPreset; }

void setCustom(uint16_t longSeconds, uint16_t shortSeconds,
               uint16_t warnAtSeconds, uint16_t warn2AtSeconds,
               uint16_t finalCountdownFrom) {
  customPreset.longSeconds = clampClock(longSeconds);
  customPreset.shortSeconds = clampClock(shortSeconds);
  customPreset.warnAtSeconds = clampMark(warnAtSeconds);
  customPreset.warn2AtSeconds = clampMark(warn2AtSeconds);
  customPreset.finalCountdownFrom = clampMark(finalCountdownFrom);

  // ONE COMMIT FOR FIVE NUMBERS. On this platform's NVS the batching is no
  // longer load-bearing -- wear levelling would make five writes harmless --
  // but the shape is kept: RefStore's set/commit split is the contract every
  // caller was written against. See RefStore.h.
  RefStore::set(RefStore::KEY_CUSTOM_LONG, customPreset.longSeconds);
  RefStore::set(RefStore::KEY_CUSTOM_SHORT, customPreset.shortSeconds);
  RefStore::set(RefStore::KEY_CUSTOM_WARN, customPreset.warnAtSeconds);
  RefStore::set(RefStore::KEY_CUSTOM_WARN2, customPreset.warn2AtSeconds);
  RefStore::set(RefStore::KEY_CUSTOM_FINAL, customPreset.finalCountdownFrom);
  RefStore::commit();
}

} // namespace RefSport
