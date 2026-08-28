#include "RefStore.h"

#include <Preferences.h>

namespace RefStore {
namespace {

// NVS key names, by Key. Short on purpose: NVS caps keys at 15 characters and
// charges flash for every byte of them.
const char *const NAMES[KEY_COUNT] = {
    "sport", "clong", "cshort", "cwarn", "cwarn2", "cfinal", "setat",
};

Preferences prefs;

// The RAM copy the rest of the firmware actually talks to.
uint32_t values[KEY_COUNT] = {};
bool     written[KEY_COUNT] = {};   // stored in NVS, or set() since boot
bool     dirty[KEY_COUNT] = {};     // set() since the last commit()
bool     anyLoaded = false;
uint32_t commits = 0;

} // namespace

void begin() {
  prefs.begin("playclock", false);
  anyLoaded = false;
  for (uint8_t k = 0; k < KEY_COUNT; k++) {
    dirty[k] = false;
    written[k] = prefs.isKey(NAMES[k]);
    values[k] = written[k] ? prefs.getUInt(NAMES[k], 0) : 0;
    anyLoaded = anyLoaded || written[k];
  }
}

bool loaded() { return anyLoaded; }

bool has(Key k) { return k < KEY_COUNT && written[k]; }

uint32_t get(Key k, uint32_t fallback) {
  if (k >= KEY_COUNT || !written[k]) {
    return fallback;
  }
  return values[k];
}

void set(Key k, uint32_t value) {
  if (k >= KEY_COUNT) {
    return;
  }
  if (written[k] && values[k] == value) {
    return;   // nothing changed; nothing to write later
  }
  values[k] = value;
  written[k] = true;
  dirty[k] = true;
}

void commit() {
  bool wrote = false;
  for (uint8_t k = 0; k < KEY_COUNT; k++) {
    if (!dirty[k]) {
      continue;
    }
    prefs.putUInt(NAMES[k], values[k]);
    dirty[k] = false;
    wrote = true;
  }
  if (wrote) {
    commits++;
  }
}

void setAndCommit(Key k, uint32_t value) {
  set(k, value);
  commit();
}

void clear() {
  prefs.clear();
  for (uint8_t k = 0; k < KEY_COUNT; k++) {
    values[k] = 0;
    written[k] = false;
    dirty[k] = false;
  }
  anyLoaded = false;
}

uint32_t writeCount() { return commits; }

} // namespace RefStore
