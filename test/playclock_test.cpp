// Host test for the play clock itself.
//
// THE STATE MACHINE THE PARENT COULD NOT TEST. In RefCounter.ino this was five
// file statics and two functions welded to esp_timer_get_time(); nothing about
// it could be exercised without an ESP32, which is why the mark precedence
// below was documented in settings.h and never checked.
//
// Two properties matter more than the rest and both are here:
//
//   THE COUNTDOWN IS RECOMPUTED, NOT ACCUMULATED. A tick that arrives late --
//   because a buzz blocked, or a frame ran long -- must land on the second the
//   wall clock says, not on the next one down. A clock that loses time is a
//   play clock that gives the offence a free second.
//
//   THE FINAL COUNTDOWN SWALLOWS A WARNING SET INSIDE IT. That is the parent's
//   documented behaviour and it is easy to reverse by accident.

#include "../src/PlayClock.h"
#include "../src/settings.h"
#include "test.h"

namespace {

// A preset built here rather than read out of the table, so these tests say
// what they mean and do not move when a sport's numbers are re-tuned.
RefSport::Preset makePreset(uint16_t lng, uint16_t shrt, uint16_t warn,
                            uint16_t warn2, uint16_t final_) {
    RefSport::Preset p = {};
    p.name = "Test";
    p.description = "test";
    p.longSeconds = lng;
    p.shortSeconds = shrt;
    p.warnAtSeconds = warn;
    p.warn2AtSeconds = warn2;
    p.finalCountdownFrom = final_;
    return p;
}

// Football as shipped: 40/25, one warning at 10, final countdown from 5.
RefSport::Preset football() { return makePreset(40, 25, 10, 0, 5); }

} // namespace

TEST(playclock, starts_idle_with_nothing_on_the_clock) {
    PlayClock c;
    c.reset();
    ASSERT_EQ(c.state(), STATE_IDLE);
    ASSERT_EQ(c.secondsLeft(), 0);
    ASSERT_EQ(c.durationSec(), 0);
}

TEST(playclock, a_tick_while_idle_does_nothing) {
    PlayClock c;
    c.reset();
    ASSERT_TRUE(!c.tick(50000));
    ASSERT_EQ(c.state(), STATE_IDLE);
}

TEST(playclock, the_full_duration_shows_for_the_first_whole_second) {
    PlayClock c;
    c.start(football(), 40, 1000);
    ASSERT_EQ(c.state(), STATE_RUNNING);
    ASSERT_EQ(c.secondsLeft(), 40);

    // 999 ms in, it is still 40 and nothing has changed.
    ASSERT_TRUE(!c.tick(1999));
    ASSERT_EQ(c.secondsLeft(), 40);

    ASSERT_TRUE(c.tick(2000));
    ASSERT_EQ(c.secondsLeft(), 39);
}

TEST(playclock, remaining_time_is_recomputed_not_accumulated) {
    // The tick that would have shown 39 never arrives -- a buzz blocked, or a
    // frame ran long. The next one must land on 35, not on 39.
    PlayClock c;
    c.start(football(), 40, 0);
    ASSERT_TRUE(c.tick(5000));
    ASSERT_EQ(c.secondsLeft(), 35);

    // And a very late tick lands on zero rather than walking down to it.
    ASSERT_TRUE(c.tick(60000));
    ASSERT_EQ(c.secondsLeft(), 0);
    ASSERT_EQ(c.state(), STATE_EXPIRED);
}

TEST(playclock, tick_reports_a_change_only_when_the_second_changes) {
    PlayClock c;
    c.start(football(), 40, 0);
    ASSERT_TRUE(c.tick(3000));
    ASSERT_TRUE(!c.tick(3100));
    ASSERT_TRUE(!c.tick(3999));
    ASSERT_TRUE(c.tick(4000));
}

TEST(playclock, expiring_leaves_zero_on_the_clock_and_holds_it) {
    PlayClock c;
    c.start(football(), 5, 0);
    ASSERT_TRUE(c.tick(5000));
    ASSERT_EQ(c.state(), STATE_EXPIRED);
    ASSERT_EQ(c.secondsLeft(), 0);

    ASSERT_TRUE(!c.expiredHoldDone(5000));
    ASSERT_TRUE(!c.expiredHoldDone(5000 + EXPIRED_HOLD_MS - 1));
    ASSERT_TRUE(c.expiredHoldDone(5000 + EXPIRED_HOLD_MS));
}

TEST(playclock, the_expired_hold_is_never_done_in_another_state) {
    // The sketch asks unconditionally, so this has to be safe to ask.
    PlayClock c;
    c.reset();
    ASSERT_TRUE(!c.expiredHoldDone(0));
    ASSERT_TRUE(!c.expiredHoldDone(0xFFFFFFFFu));
    c.start(football(), 40, 0);
    ASSERT_TRUE(!c.expiredHoldDone(1000000));
}

TEST(playclock, restarting_replaces_the_clock_from_the_new_moment) {
    PlayClock c;
    c.start(football(), 40, 0);
    ASSERT_TRUE(c.tick(10000));
    ASSERT_EQ(c.secondsLeft(), 30);

    c.start(football(), 25, 10000);
    ASSERT_EQ(c.secondsLeft(), 25);
    ASSERT_EQ(c.durationSec(), 25);
    ASSERT_TRUE(c.tick(11000));
    ASSERT_EQ(c.secondsLeft(), 24);
}

TEST(playclock, the_preset_is_captured_at_start_not_read_while_running) {
    // Changing the sport under a running clock must not move its marks. The
    // parent read RefSport::active() on every tick and nothing in its UI could
    // reach the menu mid-clock, so it was never noticed.
    PlayClock c;
    RefSport::Preset p = football();
    c.start(p, 40, 0);
    p.warnAtSeconds = 30;   // the caller's copy changes underneath

    ASSERT_TRUE(c.tick(10000));       // 30 left: not a mark under the captured
    ASSERT_EQ(c.mark().mark, MARK_NONE);
    ASSERT_TRUE(c.tick(30000));       // 10 left: the captured warning
    ASSERT_EQ(c.mark().mark, MARK_WARN);
}

TEST(playclock, the_mark_is_cleared_by_a_tick_that_changes_nothing) {
    PlayClock c;
    c.start(football(), 40, 0);
    ASSERT_TRUE(c.tick(30000));
    ASSERT_EQ(c.mark().mark, MARK_WARN);
    ASSERT_TRUE(!c.tick(30100));
    ASSERT_EQ(c.mark().mark, MARK_NONE);
}

// ---- mark precedence -------------------------------------------------------

TEST(playclock, zero_is_the_expire_buzz) {
    ASSERT_EQ(PlayClock::markFor(football(), 0).mark, MARK_EXPIRE);
    ASSERT_EQ(PlayClock::markFor(football(), 0).count, 1);
}

TEST(playclock, every_second_of_the_final_countdown_ticks) {
    const RefSport::Preset p = football();   // final from 5
    for (uint16_t s = 1; s <= 5; s++) {
        ASSERT_EQ(PlayClock::markFor(p, s).mark, MARK_TICK);
    }
    ASSERT_EQ(PlayClock::markFor(p, 6).mark, MARK_NONE);
}

TEST(playclock, the_first_warning_repeats_the_settings_count) {
    const MarkPlan m = PlayClock::markFor(football(), 10);
    ASSERT_EQ(m.mark, MARK_WARN);
    ASSERT_EQ(m.count, WARNING_BUZZ_COUNT);
}

TEST(playclock, the_second_warning_repeats_its_own_count) {
    // Lacrosse as shipped: warnings at 30 and 10.
    const RefSport::Preset p = makePreset(120, 20, 30, 10, 5);
    ASSERT_EQ(PlayClock::markFor(p, 30).count, WARNING_BUZZ_COUNT);
    ASSERT_EQ(PlayClock::markFor(p, 10).count, WARNING_BUZZ_COUNT_2);
    ASSERT_EQ(PlayClock::markFor(p, 30).mark, MARK_WARN);
    ASSERT_EQ(PlayClock::markFor(p, 10).mark, MARK_WARN);
}

TEST(playclock, a_warning_inside_the_final_countdown_is_swallowed) {
    // settings.h says so and the parent never checked it: the per-second
    // countdown wins, so the second buzzes as a TICK rather than as a warning.
    // Someone who sets Warn 1 to 3 with Final at 5 gets one click, not two.
    const RefSport::Preset p = makePreset(40, 25, 3, 0, 5);
    ASSERT_EQ(PlayClock::markFor(p, 3).mark, MARK_TICK);
    ASSERT_EQ(PlayClock::markFor(p, 3).count, 1);
}

TEST(playclock, a_mark_of_zero_means_off_not_a_mark_at_zero_seconds) {
    // Custom ships with warn2 = 0. If that were treated as "buzz at 0" it
    // would double up with the expire buzz on every single countdown.
    const RefSport::Preset p = makePreset(40, 25, 10, 0, 5);
    ASSERT_EQ(PlayClock::markFor(p, 0).mark, MARK_EXPIRE);
    ASSERT_EQ(PlayClock::markFor(p, 0).count, 1);
}

TEST(playclock, a_final_countdown_of_zero_is_off) {
    const RefSport::Preset p = makePreset(40, 25, 10, 0, 0);
    ASSERT_EQ(PlayClock::markFor(p, 1).mark, MARK_NONE);
    ASSERT_EQ(PlayClock::markFor(p, 5).mark, MARK_NONE);
    ASSERT_EQ(PlayClock::markFor(p, 10).mark, MARK_WARN);
}

TEST(playclock, the_countdown_survives_the_millis_wrap) {
    // millis() wraps every 49.7 days. A watch left running across it must not
    // decide that 40 seconds have suddenly elapsed.
    const uint32_t nearWrap = 0xFFFFF000u;
    PlayClock c;
    c.start(football(), 40, nearWrap);
    ASSERT_EQ(c.secondsLeft(), 40);

    // 5 seconds later, on the far side of the wrap.
    ASSERT_TRUE(c.tick(nearWrap + 5000));
    ASSERT_EQ(c.secondsLeft(), 35);
    ASSERT_EQ(c.state(), STATE_RUNNING);

    ASSERT_TRUE(c.tick(nearWrap + 40000));
    ASSERT_EQ(c.state(), STATE_EXPIRED);
    ASSERT_TRUE(c.expiredHoldDone(nearWrap + 40000 + EXPIRED_HOLD_MS));
}

int main() { return runAllTests(); }
