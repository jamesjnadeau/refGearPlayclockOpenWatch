// Host test for the drift reminder.
//
// The arithmetic in it is the sort that looks obvious and is not: whole
// calendar months rather than 30-day blocks, and a ppm figure that overflows
// 32 bits if multiplied before it is divided. The expectations below are for
// THE DS3231MZ'S 5 PPM, not the parent's 1 -- see RefDrift.h.

#include "../src/RefDrift.h"
#include "test.h"

#include <stdlib.h>
#include <string.h>

namespace {

// A fixed local instant, so the tests do not depend on the machine's clock.
time_t at(int year, int month, int day, int hour = 12) {
    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_isdst = -1;
    return mktime(&t);
}

} // namespace

TEST(drift, months_are_calendar_months_not_thirty_day_blocks) {
    ASSERT_EQ((int)RefDrift::monthsSince(at(2026, 1, 15), at(2026, 2, 15)), 1);
    ASSERT_EQ((int)RefDrift::monthsSince(at(2026, 1, 15), at(2026, 2, 14)), 0);
    ASSERT_EQ((int)RefDrift::monthsSince(at(2026, 1, 15), at(2027, 1, 15)), 12);
    ASSERT_EQ((int)RefDrift::monthsSince(at(2026, 11, 20), at(2027, 3, 19)), 3);
}

TEST(drift, a_month_number_that_differs_is_not_a_month_that_passed) {
    // The 31st of March to the 1st of April is one day, and the month numbers
    // differ by one. Rounding that up to "set 1 month ago" is how a reminder
    // stops being believed.
    ASSERT_EQ((int)RefDrift::monthsSince(at(2026, 3, 31), at(2026, 4, 1)), 0);
}

TEST(drift, time_running_backwards_reports_nothing_rather_than_nonsense) {
    // The user can set the clock to any date, including one before the last
    // time they set it. An unsigned subtraction there would report about
    // 4 billion months.
    ASSERT_EQ((int)RefDrift::monthsSince(at(2027, 1, 1), at(2026, 1, 1)), 0);
    ASSERT_EQ((int)RefDrift::worstCaseDriftSeconds(at(2027, 1, 1),
                                                   at(2026, 1, 1)), 0);
}

TEST(drift, a_year_at_five_ppm_is_about_two_and_a_half_minutes) {
    // 31,536,000 seconds at 5 ppm is about 158 seconds. This is the number
    // that justifies the whole module: small enough to be invisible for
    // weeks, large enough that within a season the watch is meaningfully out.
    const uint32_t s = RefDrift::worstCaseDriftSeconds(at(2026, 1, 1),
                                                       at(2027, 1, 1));
    ASSERT_TRUE(s >= 150 && s <= 165);
}

TEST(drift, a_decade_does_not_overflow) {
    // Ten years of seconds is 3.15e8, and multiplying that by a million
    // before dividing overflows 32 bits by three orders of magnitude.
    const uint32_t s = RefDrift::worstCaseDriftSeconds(at(2026, 1, 1),
                                                       at(2036, 1, 1));
    ASSERT_TRUE(s >= 1500 && s <= 1650);
}

TEST(drift, a_clock_never_set_always_reminds) {
    // Not "set a long time ago" -- a different state, and the one every
    // fresh board lands in.
    ASSERT_TRUE(RefDrift::shouldRemind(0, at(2026, 8, 23)));
    char buf[32];
    RefDrift::describe(buf, sizeof(buf), 0, at(2026, 8, 23));
    ASSERT_TRUE(strcmp(buf, "clock never set") == 0);
}

TEST(drift, the_reminder_starts_at_three_months_and_not_before) {
    const time_t setAt = at(2026, 1, 15);
    ASSERT_TRUE(!RefDrift::shouldRemind(setAt, at(2026, 4, 14)));  // 2
    ASSERT_TRUE(RefDrift::shouldRemind(setAt, at(2026, 4, 15)));   // 3
}

TEST(drift, the_description_reads_like_a_person_wrote_it) {
    char buf[32];
    const time_t setAt = at(2026, 1, 15);

    RefDrift::describe(buf, sizeof(buf), setAt, at(2026, 1, 20));
    ASSERT_TRUE(strcmp(buf, "set this month") == 0);

    RefDrift::describe(buf, sizeof(buf), setAt, at(2026, 2, 15));
    ASSERT_TRUE(strcmp(buf, "set 1 month ago") == 0);

    RefDrift::describe(buf, sizeof(buf), setAt, at(2027, 3, 15));
    ASSERT_TRUE(strcmp(buf, "set 14 months ago") == 0);
}

TEST(drift, describe_never_runs_off_the_end_of_a_short_buffer) {
    char buf[8];
    memset(buf, 'x', sizeof(buf));
    RefDrift::describe(buf, sizeof(buf), at(2020, 1, 1), at(2026, 8, 23));
    ASSERT_TRUE(buf[sizeof(buf) - 1] == '\0');
}

int main() { return runAllTests(); }
