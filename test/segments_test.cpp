// Host test for the countdown digit placement in RefSegments.cpp. Pure
// arithmetic: no Arduino headers, no panel, so it runs anywhere.
//
// PORTED FROM THE PARENT with its expectations re-derived, which is the whole
// point of the exercise. RefSegments.cpp itself needed no change at all --
// the panel it lays out for is a parameter -- but every number the parent's
// test pinned was a number about a 200 x 200 panel and a 70 px digit. Copying
// those across would have produced a suite that passed on arithmetic nobody
// draws with.
//
// The one thing carried over verbatim is the sweep: check EVERY value 0..199
// in every style the display actually uses, and print one summary line rather
// than 400 passing ones. A failing value still gets its own line.

#include "../src/RefLayout.h"
#include "../src/RefSegments.h"
#include "test.h"

TEST(segments, two_digits_are_centred) {
    const CountLayout l = layoutCount(40, STYLE_BIG, SCREEN_W);
    ASSERT_EQ(l.hundreds, false);
    ASSERT_EQ(l.tens, 4);
    ASSERT_EQ(l.ones, 0);
    ASSERT_EQ(l.width, 2 * STYLE_BIG.w + STYLE_BIG.gap);
    // Centred means equal margins either side.
    ASSERT_EQ(l.tensX, (SCREEN_W - l.width) / 2);
    ASSERT_EQ(l.onesX, l.tensX + STYLE_BIG.w + STYLE_BIG.gap);
}

TEST(segments, three_digits_add_a_skinny_one_and_shift_right) {
    const CountLayout l = layoutCount(120, STYLE_BIG, SCREEN_W);
    ASSERT_EQ(l.hundreds, true);
    ASSERT_EQ(l.tens, 2);
    ASSERT_EQ(l.ones, 0);
    ASSERT_EQ(l.width, STYLE_BIG.t + STYLE_BIG.gap
                           + 2 * STYLE_BIG.w + STYLE_BIG.gap);
    ASSERT_EQ(l.tensX, l.oneX + STYLE_BIG.t + STYLE_BIG.gap);
}

TEST(segments, the_hundreds_boundary_is_where_it_should_be) {
    ASSERT_EQ(layoutCount(99,  STYLE_BIG, SCREEN_W).hundreds, false);
    ASSERT_EQ(layoutCount(100, STYLE_BIG, SCREEN_W).hundreds, true);
}

TEST(segments, above_the_ceiling_it_clamps_rather_than_growing) {
    const CountLayout l = layoutCount(200, STYLE_BIG, SCREEN_W);
    ASSERT_EQ(l.hundreds, true);
    ASSERT_EQ(l.tens, 9);
    ASSERT_EQ(l.ones, 9);
}

TEST(segments, zero_still_draws_two_digits) {
    const CountLayout l = layoutCount(0, STYLE_BIG, SCREEN_W);
    ASSERT_EQ(l.hundreds, false);
    ASSERT_EQ(l.tens, 0);
    ASSERT_EQ(l.ones, 0);
}

// The sweep. BIG is the running countdown and SMALL is what the idle screen
// stacks -- and SMALL matters as much as BIG, because the presets that reach
// 120 are drawn there on the ready screen before anyone starts a clock.
static void sweep(const char *name, const SegStyle &s) {
    for (uint16_t v = 0; v <= SEG_MAX_VALUE; v++) {
        const CountLayout l = layoutCount(v, s, SCREEN_W);
        const int16_t left  = l.hundreds ? l.oneX : l.tensX;
        const int16_t right = l.onesX + s.w;
        if (left < 0 || right > SCREEN_W) {
            std::printf("  FAIL %s: %u spans %d..%d on a %d panel\n",
                        name, (unsigned)v, left, right, SCREEN_W);
            failures()++;
            return;
        }
    }
}

TEST(segments, every_value_fits_the_panel_in_STYLE_BIG) {
    sweep("STYLE_BIG", STYLE_BIG);
}

TEST(segments, every_value_fits_the_panel_in_STYLE_SMALL) {
    sweep("STYLE_SMALL", STYLE_SMALL);
}

TEST(segments, every_value_fits_the_panel_in_STYLE_MED) {
    sweep("STYLE_MED", STYLE_MED);
}

// The row arithmetic has to stay in sync between drawDigit and drawOneBar --
// they both ask layoutRows for the same numbers rather than each recomputing
// them, and these are the invariants that make that safe.
static void rowInvariants(const SegStyle &s) {
    const SegRows r = layoutRows(s);
    ASSERT_EQ(r.upY + r.upH, r.midY);        // upper run ends where g begins
    ASSERT_EQ(r.lowY, r.midY + s.t);         // lower run starts one t below
    ASSERT_EQ(r.lowY + r.lowH, s.h - s.t);   // and ends one t above the base
    ASSERT_TRUE(r.upH > 0);
    ASSERT_TRUE(r.lowH > 0);
}

TEST(segments, row_invariants_hold_for_every_style) {
    rowInvariants(STYLE_BIG);
    rowInvariants(STYLE_SMALL);
    rowInvariants(STYLE_MED);
}

int main() { return runAllTests(); }
