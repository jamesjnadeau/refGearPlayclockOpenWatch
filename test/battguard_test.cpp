// Host test for the low-voltage cutoff.
//
// The interesting cases here are not "does it cut at 3.2 V". They are the
// four things that stop it cutting, because batteryVolts() has never been
// measured and board.h expects the first hardware to read LOW. A cutoff that
// believes a bad reading turns a full watch into a dead one, so the refusals
// are the feature and they get the most tests.

#include "../src/BattGuard.h"
#include "../src/settings.h"
#include "test.h"

namespace {

using namespace BattGuard;

// Feed the same reading n times and return the last verdict. Most of these
// tests are about what happens over a run of samples rather than over one.
Level feed(State &s, float volts, bool charging, int n) {
    Level last = BATT_OK;
    for (int i = 0; i < n; i++) {
        last = update(s, volts, charging);
    }
    return last;
}

// A state that has already seen a healthy cell, which is the ordinary case:
// a watch that was charged, worked, and is now running down.
State armed() {
    State s;
    update(s, 4.10f, false);
    return s;
}

} // namespace

TEST(battguard, a_healthy_cell_is_ok) {
    State s;
    ASSERT_EQ((int)update(s, 4.20f, false), (int)BATT_OK);
    ASSERT_EQ((int)update(s, 3.70f, false), (int)BATT_OK);
    ASSERT_EQ((int)update(s, BATT_WARN_V, false), (int)BATT_OK);
}

TEST(battguard, below_the_warning_line_warns_without_cutting) {
    State s = armed();
    // 3.30 V is under the gauge's empty mark and over the cutoff.
    ASSERT_EQ((int)feed(s, 3.30f, false, 20), (int)BATT_LOW);
}

TEST(battguard, a_sustained_low_cell_cuts) {
    State s = armed();
    ASSERT_EQ((int)feed(s, 3.10f, false, BATT_LOW_SAMPLES), (int)BATT_CUTOFF);
}

TEST(battguard, one_low_reading_is_not_enough) {
    // A 500k source makes a single bad conversion likely, so one sample must
    // never be able to sleep the watch.
    // The cut lands ON the BATT_LOW_SAMPLES'th low reading, so every one
    // before it warns and only the last one acts.
    State s = armed();
    for (int i = 1; i < BATT_LOW_SAMPLES; i++) {
        ASSERT_EQ((int)update(s, 3.10f, false), (int)BATT_LOW);
    }
    ASSERT_EQ((int)update(s, 3.10f, false), (int)BATT_CUTOFF);
}

TEST(battguard, a_healthy_reading_clears_the_run) {
    State s = armed();
    feed(s, 3.10f, false, BATT_LOW_SAMPLES - 1);
    ASSERT_EQ((int)update(s, 3.90f, false), (int)BATT_OK);
    // The count is gone, so the next low sample starts a fresh run.
    ASSERT_EQ((int)update(s, 3.10f, false), (int)BATT_LOW);
}

TEST(battguard, an_unarmed_guard_never_cuts_however_long_it_reads_low) {
    // THE SOFT-BRICK CASE. This is a board whose divider reads low from
    // power-on -- exactly what board.h predicts for the default ADC sampling
    // time against 500k. It must warn and never sleep.
    State s;
    ASSERT_EQ((int)feed(s, 3.10f, false, 500), (int)BATT_LOW);
    ASSERT_EQ((int)s.armed, 0);
}

TEST(battguard, arming_needs_a_plainly_healthy_reading_not_a_passable_one) {
    State s;
    // Between the warning line and the arming line: good enough not to warn,
    // not good enough to be evidence the ADC works.
    feed(s, (BATT_WARN_V + BATT_ARM_V) / 2.0f, false, 10);
    ASSERT_EQ((int)s.armed, 0);
    ASSERT_EQ((int)feed(s, 3.10f, false, 50), (int)BATT_LOW);

    update(s, BATT_ARM_V, false);
    ASSERT_EQ((int)s.armed, 1);
    ASSERT_EQ((int)feed(s, 3.10f, false, BATT_LOW_SAMPLES), (int)BATT_CUTOFF);
}

TEST(battguard, charging_inhibits_the_cut_but_not_the_warning) {
    State s = armed();
    ASSERT_EQ((int)feed(s, 3.10f, true, 500), (int)BATT_LOW);
    // And unplugging does not act on the charged-up history either -- the run
    // has to be rebuilt from scratch.
    ASSERT_EQ((int)update(s, 3.10f, false), (int)BATT_LOW);
}

TEST(battguard, an_impossible_reading_is_unreadable_not_flat) {
    // At 2 V at the cell the PCM has opened and this code is not running, so
    // such a reading means the ADC path, not the battery.
    State s = armed();
    ASSERT_EQ((int)feed(s, 0.0f, false, 500), (int)BATT_UNREADABLE);
    ASSERT_EQ((int)update(s, 1.90f, false), (int)BATT_UNREADABLE);
}

TEST(battguard, an_impossible_reading_cannot_arm_the_guard) {
    State s;
    feed(s, 0.0f, false, 10);
    ASSERT_EQ((int)s.armed, 0);
}

TEST(battguard, a_nonsense_sample_breaks_a_run_of_low_ones) {
    // A run interrupted by a reading that cannot be true is not a run.
    State s = armed();
    feed(s, 3.10f, false, BATT_LOW_SAMPLES - 1);
    ASSERT_EQ((int)update(s, 0.0f, false), (int)BATT_UNREADABLE);
    ASSERT_EQ((int)update(s, 3.10f, false), (int)BATT_LOW);
}

TEST(battguard, reset_forgets_both_the_arming_and_the_run) {
    State s = armed();
    feed(s, 3.10f, false, BATT_LOW_SAMPLES - 1);
    reset(s);
    ASSERT_EQ((int)s.armed, 0);
    ASSERT_EQ((int)s.lowSamples, 0);
    ASSERT_EQ((int)feed(s, 3.10f, false, 500), (int)BATT_LOW);
}

TEST(battguard, the_cut_sits_between_the_cells_floor_and_the_gauges_empty) {
    // The numbers themselves are the point of the module, so they are pinned
    // here rather than only in settings.h. 3.0 V is the LP402025's stated
    // discharge cut-off and 2.5 V is where its PCM opens; the cut has to be
    // above both, and below the mark the gauge already calls empty.
    ASSERT_TRUE(BATT_CUTOFF_V > 3.0f);
    ASSERT_TRUE(BATT_CUTOFF_V < BATT_WARN_V);
    ASSERT_TRUE(BATT_WARN_V <= BATT_ARM_V);
    ASSERT_TRUE(BATT_IMPLAUSIBLE_V < 2.5f);
    // The warning line and the gauge's empty mark are deliberately the same
    // number: the empty gauge IS the warning.
    ASSERT_TRUE(BATT_WARN_V == BATT_MIN_V);
}

int main() { return runAllTests(); }
