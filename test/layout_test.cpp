// Host test for the panel layout.
//
// MOST OF RefLayout.h CHECKS ITSELF: it is a wall of static_asserts, and
// merely including it here fires every one of them at compile time -- the
// circle-fit checks included. This file exists for the things a static_assert
// cannot say as clearly: that the header and the body between them cover the
// panel exactly, and that the derived positions still make sense as a whole.

#include "../src/RefLayout.h"
#include "test.h"

TEST(layout, the_visible_circle_is_the_whole_panel) {
    ASSERT_EQ(SCREEN_W, 240);
    ASSERT_EQ(SCREEN_H, 240);
    ASSERT_EQ(CIRCLE_R * 2, SCREEN_W);
}

TEST(layout, the_header_is_about_the_same_fraction_of_the_panel_as_ever) {
    // The parent drew its rule at 18% of the panel; this one sits at 20%,
    // bought deliberately -- the top of a circle is the narrow part, and a
    // slightly lower rule gives the clock a wider chord. Pin it to the band
    // so a future nudge has to argue with a number.
    const double here = (double)HEADER_RULE_Y / (double)SCREEN_H;
    ASSERT_TRUE(here >= 0.17 && here <= 0.21);
}

TEST(layout, the_header_and_the_body_cover_the_panel_exactly) {
    // No footer, no reserved strip, no margin held back. The body runs from
    // the pixel under the rule to the last row of the panel.
    ASSERT_EQ(BODY_Y0, HEADER_RULE_Y + 1);
    ASSERT_EQ(BODY_Y1, SCREEN_H - 1);
    ASSERT_EQ(BODY_H, SCREEN_H - HEADER_RULE_Y - 1);
    ASSERT_EQ(HEADER_RULE_Y + 1 + BODY_H, SCREEN_H);
}

TEST(layout, the_clock_and_the_gauge_do_not_collide) {
    ASSERT_TRUE(CLOCK_X + CLOCK_W < BATT_X);
    // And there is real space between them, not a shared pixel.
    ASSERT_TRUE(BATT_X - (CLOCK_X + CLOCK_W) >= 8);
}

TEST(layout, the_big_countdown_is_vertically_centred_in_the_body) {
    const int16_t above = BIG_Y - BODY_Y0;
    const int16_t below = BODY_Y1 - (BIG_Y + STYLE_BIG.h - 1);
    // Centred to within a pixel, since the body height may be odd.
    ASSERT_TRUE(above - below <= 1 && below - above <= 1);
}

TEST(layout, both_idle_rows_sit_inside_the_body) {
    ASSERT_TRUE(ROW1_Y >= BODY_Y0);
    ASSERT_TRUE(ROW2_Y + STYLE_SMALL.h - 1 <= BODY_Y1);
    ASSERT_TRUE(ROW1_Y + STYLE_SMALL.h < ROW2_Y);
}

TEST(layout, the_row_marker_does_not_reach_the_digits) {
    // The marker hangs at the right edge; the widest a SMALL count gets is
    // its full pair plus a hundreds bar, centred.
    const int16_t widest = STYLE_SMALL.t + STYLE_SMALL.gap
                           + 2 * STYLE_SMALL.w + STYLE_SMALL.gap;
    const int16_t rightEdge = (SCREEN_W + widest) / 2;
    ASSERT_TRUE(rightEdge < MARKER_TIP_X - MARKER_W);
}

TEST(layout, the_idle_rows_line_up_with_the_right_hand_buttons) {
    // The OSW's two right-hand buttons sit at roughly y = 44 and y = 190 of
    // the panel (open-smartwatch-os draws its own button hints there). Each
    // row's marker should sit in the button's half of the body, or the
    // arrows point at nothing.
    const int16_t row1Centre = ROW1_Y + STYLE_SMALL.h / 2;
    const int16_t row2Centre = ROW2_Y + STYLE_SMALL.h / 2;
    ASSERT_TRUE(row1Centre < SCREEN_H / 2);
    ASSERT_TRUE(row2Centre > SCREEN_H / 2);
}

TEST(layout, the_digit_styles_are_ordered_big_med_small) {
    // Not cosmetic: the menu's set-time field has to read as smaller than a
    // running clock and larger than a stacked idle row, or the three screens
    // stop being distinguishable at a glance.
    ASSERT_TRUE(STYLE_BIG.w > STYLE_MED.w);
    ASSERT_TRUE(STYLE_MED.w > STYLE_SMALL.w);
    ASSERT_TRUE(STYLE_BIG.h > STYLE_MED.h);
    ASSERT_TRUE(STYLE_MED.h > STYLE_SMALL.h);
}

TEST(layout, the_circle_test_itself_tells_the_truth) {
    // The centre is visible, the corners are not, and the boundary is inside.
    ASSERT_TRUE(pointVisible(120, 120));
    ASSERT_TRUE(pointVisible(120, 0));
    ASSERT_TRUE(pointVisible(0, 120));
    ASSERT_TRUE(!pointVisible(0, 0));
    ASSERT_TRUE(!pointVisible(239, 239));
    ASSERT_TRUE(!rectVisible(0, 0, 239, 239));
    ASSERT_TRUE(rectVisible(60, 60, 120, 120));
}

int main() { return runAllTests(); }
