#ifndef REF_BATT_GUARD_H
#define REF_BATT_GUARD_H

#include <stdint.h>

// The low-voltage cutoff, ported whole because its safety argument is the
// part that transfers between boards.
//
// WHY IT IS KEPT ON THIS PLATFORM. A LiPo left to run a watch until its
// protection circuit opens is parked below its rated discharge floor; the
// firmware sleeping the watch first is cheap insurance on any board whose
// hardware does not guarantee otherwise -- and this project has not seen the
// OSW's schematic guarantee it.
//
// *** AND WHY IT CANNOT SIMPLY TRUST THE READING. *** batteryVolts() has
// never been measured ON THIS BOARD, and the evidence available says not to
// trust it: the shipped open-smartwatch firmware never converts the divider
// reading to volts at all -- it self-calibrates raw ADC counts against the
// lowest and highest values it has ever observed, which is what a project
// does when the absolute reading is not dependable. So the first hardware
// this runs on may well read low, and a naive cutoff would put a full watch
// to sleep, wake on the hold, read low again and sleep again. That is a watch
// that looks dead. It is a worse failure than the one being fixed: an
// over-discharged cell loses life, a bricked watch loses the watch.
//
// SO THE GUARD ARMS ITSELF ON EVIDENCE. It will not force sleep until it has
// seen this ADC return a plainly healthy cell (>= BATT_ARM_V) at least once
// since boot. A board whose divider reads low from power-on never satisfies
// that and so never sleeps -- it warns, forever, which is exactly the signal
// bring-up wants. A board that read a healthy cell and then genuinely ran it
// down does satisfy it, and gets the protection. The precondition is what
// makes an unproven input safe to act on, and it costs one bool.
//
// FOUR OTHER THINGS HOLD IT BACK, all of them cheap:
//
//   - A reading under BATT_IMPLAUSIBLE_V is reported as UNREADABLE rather
//     than as a flat cell. At 2 V at the cell the PCM has long since opened
//     and this code is not running, so such a reading is a broken ADC path
//     and never an emergency.
//   - Charging inhibits the cut. A watch on USB is never put to sleep by
//     this, which also means bring-up at a bench cannot be locked out.
//   - BATT_LOW_SAMPLES consecutive low readings are required, so one bad
//     conversion -- which a 500 kOhm source makes likely -- cannot act.
//   - Any reading at or above BATT_WARN_V clears the count outright.
//
// PURE LOGIC, no Arduino and no ADC, like RefSegments and RefDrift: the
// caller passes in the volts and whether the charger is pulling STAT low, and
// the whole state machine is host-tested in test/battguard_test.cpp.
//
// WHAT IT DELIBERATELY DOES NOT DO. It does not decide what LOW looks like on
// the panel. The gauge in RefDisplay already empties at BATT_MIN_V, which is
// the same 3.40 V as BATT_WARN_V, so the warning is drawn already and this
// module does not need a second opinion about it.
namespace BattGuard {

enum Level {
    // Above the warning line, or held there by one of the inhibits.
    BATT_OK,
    // Below the warning line. Keep running; the gauge is already empty.
    BATT_LOW,
    // Sustained below the cutoff, not charging, and the ADC has proven
    // itself. The caller should sleep.
    BATT_CUTOFF,
    // A reading the board could not be running at. The ADC path is broken,
    // not the cell. Never an emergency; worth saying once.
    BATT_UNREADABLE,
};

// Everything the guard remembers between samples. Light sleep retains SRAM,
// so a watch that armed before it slept is still armed when it wakes -- which
// is right, because it is the same ADC and the same cell.
struct State {
    bool    armed      = false;
    uint8_t lowSamples = 0;
};

// Feed one reading. `volts` is at the CELL (batteryVolts() already undoes the
// divider); `charging` is true when the MCP73832 is pulling STAT low.
Level update(State &s, float volts, bool charging);

// Forget the sustained-low count and the arming evidence. For tests and for
// anything that wants a clean slate; the main loop never calls it.
void reset(State &s);

} // namespace BattGuard

#endif // REF_BATT_GUARD_H
