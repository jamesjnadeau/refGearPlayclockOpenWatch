#pragma once

// Host stub for the ESP32 Preferences (NVS) library.
//
// Enough of a key-value store to exercise RefStore off the watch: values
// persist across a Preferences object's lifetime (they live in a process-wide
// map, the way NVS lives in flash), isKey() reports genuine existence, and
// clear() wipes the namespace. What it does NOT model: wear, namespaces
// beyond one, or types beyond the uint32 RefStore uses.

#include <cstdint>
#include <cstring>
#include <map>
#include <string>

namespace PreferencesStub {

// One namespace's worth of storage, process-wide -- "flash".
inline std::map<std::string, uint32_t> &flash() {
    static std::map<std::string, uint32_t> f;
    return f;
}

// Wipe the fake flash entirely. For tests that want a factory-fresh board.
inline void wipe() { flash().clear(); }

} // namespace PreferencesStub

class Preferences {
public:
    bool begin(const char *, bool = false) { return true; }
    void end() {}

    bool isKey(const char *key) {
        return PreferencesStub::flash().count(key) != 0;
    }

    uint32_t getUInt(const char *key, uint32_t fallback = 0) {
        auto it = PreferencesStub::flash().find(key);
        return it == PreferencesStub::flash().end() ? fallback : it->second;
    }

    size_t putUInt(const char *key, uint32_t value) {
        PreferencesStub::flash()[key] = value;
        return sizeof(value);
    }

    bool clear() {
        PreferencesStub::flash().clear();
        return true;
    }
};
