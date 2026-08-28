// Host test for the settings store, against the Preferences (NVS) stub.
//
// The interesting property is not "does it remember a number" -- it is what
// happens when it CANNOT. A fresh board, and any key never written, must read
// as ABSENT, so every module falls back to its settings.h default. A store
// that hands back a plausible number out of an unwritten key is worse than
// one that fails, because nothing downstream can tell. The per-key existence
// contract survives the move from the parent's emulated flash to NVS, and
// this suite is what says so.

#include "../src/RefStore.h"
#include "test.h"

namespace {

// A board that has never been written. clear() is a real feature -- a factory
// reset row would call it -- so the tests use the same path the watch would.
void freshBoard() {
    RefStore::begin();
    RefStore::clear();
    RefStore::begin();
}

} // namespace

TEST(store, a_value_survives_a_power_cycle) {
    freshBoard();
    RefStore::setAndCommit(RefStore::KEY_SPORT, 3);
    RefStore::begin();                       // the power cycle
    ASSERT_TRUE(RefStore::loaded());
    ASSERT_EQ((int)RefStore::get(RefStore::KEY_SPORT, 99), 3);
}

TEST(store, every_key_is_independent) {
    freshBoard();
    RefStore::set(RefStore::KEY_CUSTOM_LONG, 40);
    RefStore::set(RefStore::KEY_CUSTOM_SHORT, 25);
    RefStore::set(RefStore::KEY_CUSTOM_WARN, 10);
    RefStore::commit();
    RefStore::begin();
    ASSERT_EQ((int)RefStore::get(RefStore::KEY_CUSTOM_LONG, 0), 40);
    ASSERT_EQ((int)RefStore::get(RefStore::KEY_CUSTOM_SHORT, 0), 25);
    ASSERT_EQ((int)RefStore::get(RefStore::KEY_CUSTOM_WARN, 0), 10);
    // Untouched, so it falls back rather than reading as zero.
    ASSERT_EQ((int)RefStore::get(RefStore::KEY_CUSTOM_WARN2, 7), 7);
}

TEST(store, a_full_epoch_round_trips) {
    freshBoard();
    // KEY_CLOCK_SET_AT holds a time_t. A 16-bit slot would have been enough
    // for everything else and would have silently truncated this.
    RefStore::begin();
    const uint32_t epoch = 1787000000UL;   // some time in 2026
    RefStore::setAndCommit(RefStore::KEY_CLOCK_SET_AT, epoch);
    RefStore::begin();
    ASSERT_TRUE(RefStore::get(RefStore::KEY_CLOCK_SET_AT, 0) == epoch);
}

TEST(store, setting_the_same_value_again_costs_no_write) {
    // The one that protects the flash page. A settings menu that re-saves
    // everything on the way out must not spend an erase cycle doing it.
    RefStore::begin();
    RefStore::setAndCommit(RefStore::KEY_SPORT, 5);
    const uint32_t before = RefStore::writeCount();
    RefStore::setAndCommit(RefStore::KEY_SPORT, 5);
    RefStore::setAndCommit(RefStore::KEY_SPORT, 5);
    ASSERT_EQ((int)(RefStore::writeCount() - before), 0);
}

TEST(store, several_changes_commit_as_one_write) {
    // Five numbers changed by the Custom editor is one page erase, not five.
    RefStore::begin();
    RefStore::setAndCommit(RefStore::KEY_CUSTOM_LONG, 1);
    const uint32_t before = RefStore::writeCount();

    RefStore::set(RefStore::KEY_CUSTOM_LONG, 60);
    RefStore::set(RefStore::KEY_CUSTOM_SHORT, 30);
    RefStore::set(RefStore::KEY_CUSTOM_WARN, 15);
    RefStore::set(RefStore::KEY_CUSTOM_WARN2, 5);
    RefStore::set(RefStore::KEY_CUSTOM_FINAL, 3);
    RefStore::commit();

    ASSERT_EQ((int)(RefStore::writeCount() - before), 1);
    RefStore::begin();
    ASSERT_EQ((int)RefStore::get(RefStore::KEY_CUSTOM_FINAL, 0), 3);
}

TEST(store, a_commit_with_nothing_dirty_is_not_a_write) {
    RefStore::begin();
    RefStore::setAndCommit(RefStore::KEY_SPORT, 2);
    const uint32_t before = RefStore::writeCount();
    RefStore::commit();
    RefStore::commit();
    ASSERT_EQ((int)(RefStore::writeCount() - before), 0);
}

TEST(store, an_unwritten_key_hands_back_the_callers_fallback) {
    // THE ONE THAT CAUGHT A REAL FLAW. Before the written bitmap existed, a
    // record holding one saved sport index reported every OTHER key as zero
    // -- which is a legal sport index, a legal warning threshold, and an
    // illegal clock length that would be clamped up to 1. Saving anything at
    // all would have silently replaced every settings.h default.
    freshBoard();
    RefStore::setAndCommit(RefStore::KEY_SPORT, 1);
    RefStore::begin();

    ASSERT_TRUE(RefStore::loaded());
    ASSERT_TRUE(RefStore::has(RefStore::KEY_SPORT));
    ASSERT_TRUE(!RefStore::has(RefStore::KEY_CUSTOM_FINAL));
    ASSERT_EQ((int)RefStore::get(RefStore::KEY_CUSTOM_FINAL, 5), 5);
}

TEST(store, a_board_that_has_never_been_written_reads_as_absent) {
    freshBoard();
    // clear() writes a valid but empty record, so the store IS loaded --
    // and every key still falls back.
    ASSERT_EQ((int)RefStore::get(RefStore::KEY_SPORT, 42), 42);
    ASSERT_EQ((int)RefStore::get(RefStore::KEY_CUSTOM_LONG, 40), 40);
    ASSERT_TRUE(!RefStore::has(RefStore::KEY_SPORT));
}

TEST(store, zero_is_a_value_like_any_other) {
    // Writing 0 must be distinguishable from never having written. This is
    // exactly the case a bare "is it zero" check gets wrong, and 0 is the
    // Football preset's index.
    freshBoard();
    RefStore::setAndCommit(RefStore::KEY_SPORT, 0);
    RefStore::begin();
    ASSERT_TRUE(RefStore::has(RefStore::KEY_SPORT));
    ASSERT_EQ((int)RefStore::get(RefStore::KEY_SPORT, 42), 0);
}

TEST(store, writes_are_counted_so_the_write_pattern_is_measurable) {
    // Not a behaviour so much as a promise that the question can be asked --
    // NVS wear-levels, but a caller that committed on every button press
    // would still deserve to be noticed.
    //
    // freshBoard() first, and it matters: without it the i = 0 pass writes a
    // value the store already held, which is correctly not a write at all --
    // and the count comes back nine.
    freshBoard();
    const uint32_t before = RefStore::writeCount();
    for (uint32_t i = 0; i < 10; i++) {
        RefStore::setAndCommit(RefStore::KEY_SPORT, i);
    }
    ASSERT_EQ((int)(RefStore::writeCount() - before), 10);
}

int main() {
    freshBoard();
    return runAllTests();
}
