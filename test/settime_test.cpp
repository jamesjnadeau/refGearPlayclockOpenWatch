// Setting the clock by hand is now the ONLY way this watch learns the time.
// It runs about once a year at +/-1 ppm, and again after every full discharge,
// because VBACKUP is tied off and the clock does not survive the cell being
// disconnected. Which means the user will have forgotten how it works every
// single time -- so it has to be obvious, and it has to be impossible to leave
// in a half-set state.
//
// The eight tests the plan wrote are all here, unchanged in substance. The
// rest are the edges they do not reach, and two of them found real bugs -- see
// the year-change and shift-backwards cases.

#include "../src/RefSetTime.h"
#include "test.h"

TEST(settime, starts_on_the_hour_field) {
    RefSetTime s;
    s.begin(2026, 8, 23, 14, 30);
    ASSERT_EQ(s.field(), RefSetTime::FIELD_HOUR);
}

TEST(settime, up_wraps_the_hour_at_23) {
    RefSetTime s;
    s.begin(2026, 8, 23, 23, 30);
    s.up();
    ASSERT_EQ(s.hour(), 0);
}

TEST(settime, down_wraps_the_hour_at_0) {
    RefSetTime s;
    s.begin(2026, 8, 23, 0, 30);
    s.down();
    ASSERT_EQ(s.hour(), 23);
}

TEST(settime, advance_walks_every_field_then_commits) {
    RefSetTime s;
    s.begin(2026, 8, 23, 14, 30);
    ASSERT_EQ(s.field(), RefSetTime::FIELD_HOUR);
    s.advance();
    ASSERT_EQ(s.field(), RefSetTime::FIELD_MINUTE);
    s.advance();
    ASSERT_EQ(s.field(), RefSetTime::FIELD_YEAR);
    s.advance();
    ASSERT_EQ(s.field(), RefSetTime::FIELD_MONTH);
    s.advance();
    ASSERT_EQ(s.field(), RefSetTime::FIELD_DAY);
    s.advance();
    ASSERT_TRUE(s.committed());
}

TEST(settime, day_is_clamped_to_the_month) {
    RefSetTime s;
    s.begin(2026, 1, 31, 12, 0);   // 31 January
    s.setMonth(2);                 // -> February, which has no 31st
    ASSERT_EQ(s.day(), 28);
}

TEST(settime, day_is_clamped_to_february_in_a_leap_year) {
    RefSetTime s;
    s.begin(2024, 1, 31, 12, 0);
    s.setMonth(2);
    ASSERT_EQ(s.day(), 29);
}

TEST(settime, dst_shift_moves_an_hour_without_touching_the_date) {
    RefSetTime s;
    s.begin(2026, 8, 23, 14, 30);
    s.shiftHour(+1);
    ASSERT_EQ(s.hour(), 15);
    ASSERT_EQ(s.day(), 23);
}

TEST(settime, dst_shift_across_midnight_rolls_the_date) {
    RefSetTime s;
    s.begin(2026, 8, 23, 23, 30);
    s.shiftHour(+1);
    ASSERT_EQ(s.hour(), 0);
    ASSERT_EQ(s.day(), 24);
}

// --- beyond the plan's eight ------------------------------------------------

TEST(settime, minutes_wrap_at_both_ends) {
    RefSetTime s;
    s.begin(2026, 8, 23, 12, 59);
    s.advance();                       // -> minute
    s.up();
    ASSERT_EQ(s.minute(), 0);
    s.down();
    ASSERT_EQ(s.minute(), 59);
}

TEST(settime, a_year_change_can_move_february_too) {
    // 29 February in a leap year, then step the year. The 29th does not exist
    // in 2025, and an RTC handed a date that does not exist is an RTC holding
    // a date nobody can reason about afterwards.
    RefSetTime s;
    s.begin(2024, 2, 29, 12, 0);
    s.advance(); s.advance();          // -> year
    s.up();
    ASSERT_EQ(s.year(), 2025);
    ASSERT_EQ(s.day(), 28);
}

TEST(settime, the_year_is_bounded_by_what_the_rtc_can_store) {
    // The RV-3028 has no century bit: its year register runs 00..99 and its
    // leap year rule is specified for 2000..2099 only. A year outside that
    // cannot be stored, so it must not be offered.
    RefSetTime s;
    s.begin(RefSetTime::YEAR_MAX, 6, 15, 12, 0);
    s.advance(); s.advance();          // -> year
    s.up();
    ASSERT_EQ(s.year(), RefSetTime::YEAR_MIN);
    s.down();
    ASSERT_EQ(s.year(), RefSetTime::YEAR_MAX);
}

TEST(settime, the_day_field_wraps_within_the_current_month) {
    RefSetTime s;
    s.begin(2026, 4, 30, 12, 0);       // April has 30
    s.advance(); s.advance(); s.advance(); s.advance();  // -> day
    s.up();
    ASSERT_EQ(s.day(), 1);
    s.down();
    ASSERT_EQ(s.day(), 30);
}

TEST(settime, shifting_back_across_midnight_rolls_the_date_back) {
    RefSetTime s;
    s.begin(2026, 8, 23, 0, 30);
    s.shiftHour(-1);
    ASSERT_EQ(s.hour(), 23);
    ASSERT_EQ(s.day(), 22);
}

TEST(settime, shifting_back_across_a_month_boundary) {
    RefSetTime s;
    s.begin(2026, 3, 1, 0, 30);
    s.shiftHour(-1);
    ASSERT_EQ(s.hour(), 23);
    ASSERT_EQ(s.month(), 2);
    ASSERT_EQ(s.day(), 28);            // 2026 is not a leap year
}

TEST(settime, shifting_back_across_new_year) {
    RefSetTime s;
    s.begin(2026, 1, 1, 0, 0);
    s.shiftHour(-1);
    ASSERT_EQ(s.year(), 2025);
    ASSERT_EQ(s.month(), 12);
    ASSERT_EQ(s.day(), 31);
    ASSERT_EQ(s.hour(), 23);
}

TEST(settime, shifting_forward_across_new_year) {
    RefSetTime s;
    s.begin(2026, 12, 31, 23, 0);
    s.shiftHour(+1);
    ASSERT_EQ(s.year(), 2027);
    ASSERT_EQ(s.month(), 1);
    ASSERT_EQ(s.day(), 1);
    ASSERT_EQ(s.hour(), 0);
}

TEST(settime, committing_is_final) {
    // No cancel, and no way back. A set-time screen that can be left
    // half-finished leaves the watch showing a plausible wrong time.
    RefSetTime s;
    s.begin(2026, 8, 23, 14, 30);
    for (int i = 0; i < RefSetTime::FIELD_COUNT; i++) {
        s.advance();
    }
    ASSERT_TRUE(s.committed());
    s.advance();
    ASSERT_TRUE(s.committed());
}

TEST(settime, a_nonsense_seed_is_made_sane_rather_than_stored) {
    // begin() is fed whatever the RTC said, and an unset RV-3028 says month 0.
    RefSetTime s;
    s.begin(1970, 0, 0, 99, 99);
    ASSERT_TRUE(s.year() >= RefSetTime::YEAR_MIN);
    ASSERT_TRUE(s.month() >= 1 && s.month() <= 12);
    ASSERT_TRUE(s.day() >= 1 && s.day() <= 31);
    ASSERT_TRUE(s.hour() <= 23);
    ASSERT_TRUE(s.minute() <= 59);
}

TEST(settime, leap_years_are_the_gregorian_rule_not_every_four) {
    ASSERT_TRUE(RefSetTime::isLeapYear(2024));
    ASSERT_TRUE(!RefSetTime::isLeapYear(2026));
    ASSERT_TRUE(!RefSetTime::isLeapYear(2100));   // divisible by 100
    ASSERT_TRUE(RefSetTime::isLeapYear(2000));    // but by 400
    ASSERT_EQ(RefSetTime::daysInMonth(2024, 2), 29);
    ASSERT_EQ(RefSetTime::daysInMonth(2026, 2), 28);
    ASSERT_EQ(RefSetTime::daysInMonth(2026, 4), 30);
    ASSERT_EQ(RefSetTime::daysInMonth(2026, 12), 31);
}

TEST(set_time, back_walks_the_cursor_and_is_inert_on_the_first_field) {
    // Added with the menu in Task 25 Step 3. It is a CURSOR MOVE, not a
    // cancel: a BACK that could also abandon the screen would abandon it by
    // accident, and a half-set clock is the one outcome this class exists to
    // prevent.
    RefSetTime t;
    t.begin(2026, 8, 24, 13, 45);

    t.back();
    ASSERT_EQ(t.field(), RefSetTime::FIELD_HOUR);
    ASSERT_TRUE(!t.committed());

    t.advance();
    t.advance();
    ASSERT_EQ(t.field(), RefSetTime::FIELD_YEAR);
    t.back();
    ASSERT_EQ(t.field(), RefSetTime::FIELD_MINUTE);
    t.back();
    ASSERT_EQ(t.field(), RefSetTime::FIELD_HOUR);
}

TEST(set_time, back_cannot_reopen_a_committed_edit) {
    RefSetTime t;
    t.begin(2026, 8, 24, 13, 45);
    for (uint8_t i = 0; i < RefSetTime::FIELD_COUNT; i++) {
        t.advance();
    }
    ASSERT_TRUE(t.committed());
    t.back();
    ASSERT_TRUE(t.committed());
    ASSERT_EQ(t.field(), RefSetTime::FIELD_DAY);
}

int main() { return runAllTests(); }
