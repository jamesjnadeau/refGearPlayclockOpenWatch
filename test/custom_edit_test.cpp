// Host test for the Custom preset editor.
//
// WHY THIS FILE EXISTS. The parent's editCustom() was ninety lines of drawing
// with the field walk and the wrap rules threaded through it, so none of the
// rules could be checked without a watch. They are not obvious rules, and one
// of them is asymmetric in a way that is easy to get backwards: THE TWO CLOCKS
// BOTTOM OUT AT 1 AND THE THREE MARKS DO NOT. A mark of 0 means "off", which
// somebody actively wants; a clock of 0 would expire the instant it started.

#include "../src/RefCustomEdit.h"
#include "test.h"

#include <string.h>

namespace {

RefSport::Preset makePreset(uint16_t lng, uint16_t shrt, uint16_t warn,
                            uint16_t warn2, uint16_t final_) {
    RefSport::Preset p = {};
    p.name = "Custom";
    p.description = "user set";
    p.longSeconds = lng;
    p.shortSeconds = shrt;
    p.warnAtSeconds = warn;
    p.warn2AtSeconds = warn2;
    p.finalCountdownFrom = final_;
    return p;
}

RefCustomEdit seeded() {
    RefCustomEdit e;
    e.begin(makePreset(40, 25, 10, 0, 5));
    return e;
}

void advanceTo(RefCustomEdit &e, RefCustomEdit::Field f) {
    while (e.field() != f && !e.committed()) {
        e.advance();
    }
}

} // namespace

TEST(custom_edit, seeds_from_the_preset_and_starts_on_the_long_clock) {
    RefCustomEdit e = seeded();
    ASSERT_EQ(e.field(), RefCustomEdit::FIELD_LONG);
    ASSERT_TRUE(!e.committed());
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_LONG), 40);
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_SHORT), 25);
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_WARN), 10);
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_WARN2), 0);
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_FINAL), 5);
}

TEST(custom_edit, every_field_has_a_label) {
    for (uint8_t f = 0; f < RefCustomEdit::FIELD_COUNT; f++) {
        const char *l = RefCustomEdit::label((RefCustomEdit::Field)f);
        ASSERT_TRUE(l != nullptr);
        ASSERT_TRUE(strlen(l) > 0);
        // The row draws label on the left and a three digit value on the
        // right of a 128 px panel. Six characters is what that leaves.
        ASSERT_TRUE(strlen(l) <= 6);
    }
}

TEST(custom_edit, the_two_clocks_bottom_out_at_one) {
    RefCustomEdit e = seeded();
    ASSERT_EQ(RefCustomEdit::floorOf(RefCustomEdit::FIELD_LONG),
              RefSport::MIN_CLOCK_SECONDS);
    ASSERT_EQ(RefCustomEdit::floorOf(RefCustomEdit::FIELD_SHORT),
              RefSport::MIN_CLOCK_SECONDS);

    for (int i = 0; i < 39; i++) {
        e.down();
    }
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_LONG), 1);
    // One more wraps to the top rather than reaching 0.
    e.down();
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_LONG), RefSport::MAX_SECONDS);
}

TEST(custom_edit, the_three_marks_can_be_turned_off) {
    // A mark of 0 is "off" and must be reachable, or the second warning
    // cannot be silenced on the one preset that can change it.
    RefCustomEdit e = seeded();
    for (uint8_t f = RefCustomEdit::FIELD_WARN;
         f <= RefCustomEdit::FIELD_FINAL; f++) {
        ASSERT_EQ(RefCustomEdit::floorOf((RefCustomEdit::Field)f), 0);
    }

    advanceTo(e, RefCustomEdit::FIELD_WARN);
    for (int i = 0; i < 10; i++) {
        e.down();
    }
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_WARN), 0);
    e.down();
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_WARN), RefSport::MAX_SECONDS);
}

TEST(custom_edit, the_top_of_every_field_wraps_to_its_own_floor) {
    RefCustomEdit e = seeded();
    // Long: 199 -> 1
    for (int i = 0; i < 159; i++) {
        e.up();
    }
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_LONG), RefSport::MAX_SECONDS);
    e.up();
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_LONG), RefSport::MIN_CLOCK_SECONDS);

    // Warn 2: 199 -> 0
    advanceTo(e, RefCustomEdit::FIELD_WARN2);
    for (int i = 0; i < 199; i++) {
        e.up();
    }
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_WARN2), RefSport::MAX_SECONDS);
    e.up();
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_WARN2), 0);
}

TEST(custom_edit, up_and_down_only_touch_the_current_field) {
    RefCustomEdit e = seeded();
    e.up();
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_LONG), 41);
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_SHORT), 25);
    e.advance();
    e.down();
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_LONG), 41);
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_SHORT), 24);
}

TEST(custom_edit, back_walks_the_cursor_and_is_inert_on_the_first_field) {
    RefCustomEdit e = seeded();
    e.back();
    ASSERT_EQ(e.field(), RefCustomEdit::FIELD_LONG);
    ASSERT_TRUE(!e.committed());

    e.advance();
    e.advance();
    ASSERT_EQ(e.field(), RefCustomEdit::FIELD_WARN);
    e.back();
    ASSERT_EQ(e.field(), RefCustomEdit::FIELD_SHORT);
}

TEST(custom_edit, only_advancing_past_the_last_field_commits) {
    RefCustomEdit e = seeded();
    for (uint8_t i = 0; i + 1 < RefCustomEdit::FIELD_COUNT; i++) {
        e.advance();
        ASSERT_TRUE(!e.committed());
    }
    ASSERT_EQ(e.field(), RefCustomEdit::FIELD_FINAL);
    e.advance();
    ASSERT_TRUE(e.committed());
}

TEST(custom_edit, nothing_changes_a_committed_edit) {
    // The menu reads the values out after the loop ends. A stray edge landing
    // between the commit and the read must not move anything.
    RefCustomEdit e = seeded();
    for (uint8_t i = 0; i < RefCustomEdit::FIELD_COUNT; i++) {
        e.advance();
    }
    ASSERT_TRUE(e.committed());
    const RefCustomEdit::Field f = e.field();

    e.up();
    e.down();
    e.advance();
    e.back();
    ASSERT_EQ(e.field(), f);
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_FINAL), 5);
}

TEST(custom_edit, a_preset_stored_out_of_range_is_pulled_back_in) {
    // RefSport clamps on the way in, so this should not happen -- but seeding
    // the cursor onto a value the wrap arithmetic cannot reach again would be
    // a field the user could not fix.
    RefCustomEdit e;
    e.begin(makePreset(0, 500, 500, 0, 0));
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_LONG),
              RefSport::MIN_CLOCK_SECONDS);
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_SHORT), RefSport::MAX_SECONDS);
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_WARN), RefSport::MAX_SECONDS);
}

TEST(custom_edit, an_out_of_range_field_reads_zero_rather_than_rubbish) {
    RefCustomEdit e = seeded();
    ASSERT_EQ(e.value(RefCustomEdit::FIELD_COUNT), 0);
}

int main() { return runAllTests(); }
