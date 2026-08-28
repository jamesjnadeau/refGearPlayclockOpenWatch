#include "BattGuard.h"

#include "settings.h"

namespace BattGuard {

void reset(State &s) {
    s.armed = false;
    s.lowSamples = 0;
}

Level update(State &s, float volts, bool charging) {
    // A reading the board could not be running at is a broken ADC path, not a
    // flat cell. Checked FIRST, before anything else looks at the number, so
    // that a divider reading near zero can neither arm the guard nor count
    // towards a cutoff. The sustained-low count is dropped as well: a run of
    // low samples interrupted by a nonsense one is not a run.
    if (volts < BATT_IMPLAUSIBLE_V) {
        s.lowSamples = 0;
        return BATT_UNREADABLE;
    }

    // The arming evidence. Once this ADC has returned a plainly healthy cell
    // it has shown it can, and the guard is allowed to believe it later. This
    // is the whole safety argument -- see the header.
    if (volts >= BATT_ARM_V) {
        s.armed = true;
    }

    if (volts >= BATT_WARN_V) {
        s.lowSamples = 0;
        return BATT_OK;
    }

    // Below the warning line from here down. Everything below returns LOW at
    // least, because the cell really is into the knee of the curve.

    // Charging inhibits the cut but not the warning. A cell being charged is
    // rising, so sleeping it would be both wrong and untimely -- and this is
    // what keeps a bench with USB plugged in from ever being locked out.
    if (charging) {
        s.lowSamples = 0;
        return BATT_LOW;
    }

    if (volts >= BATT_CUTOFF_V) {
        s.lowSamples = 0;
        return BATT_LOW;
    }

    // Below the cutoff, not charging. Two things still have to be true.

    // Never acted on an ADC that has not proven itself. A board whose divider
    // reads low from power-on lands here forever and warns forever, which is
    // the bring-up signal rather than a brick.
    if (!s.armed) {
        return BATT_LOW;
    }

    if (s.lowSamples < BATT_LOW_SAMPLES) {
        s.lowSamples++;
    }
    return (s.lowSamples >= BATT_LOW_SAMPLES) ? BATT_CUTOFF : BATT_LOW;
}

} // namespace BattGuard
