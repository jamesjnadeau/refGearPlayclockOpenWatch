// Host test for the debouncer.
//
// The original firmware had no test for this file, and it is the one most
// worth having. Buttons decides whether a sleeve brushed against the case can
// reset a play clock in the middle of a game, and that is not a property you
// establish by pressing a button a few times on a bench. The stub has a fake
// clock and fake pins, and time only moves when a test moves it, so a 30 ms
// debounce window and a 500 ms hold are exactly reproducible.
//
// WHAT CHANGED IN THIS PORT'S TESTS. Three buttons with MIXED active levels
// (SELECT presses LOW, the right-hand pair press HIGH -- board.h), so press()
// and release() read the role's own level. The wake-hold pair now exercises
// SELECT, because sleep moved onto the bottom-left button. And releasedAfter()
// grew its own suite: it is what splits that button's short hold (menu/clear)
// from its long one (sleep), so its window arithmetic is exactly the kind of
// off-by-one worth pinning.

#include "../src/Buttons.h"
#include "../src/board.h"
#include "../src/settings.h"
#include "test.h"

namespace {

// Pins and active levels, in Buttons::Id order.
const uint8_t PINS[Buttons::COUNT] = {
    BTN_LONG_TIMER_PIN, BTN_SHORT_TIMER_PIN, BTN_SELECT_PIN,
};
const int ACTIVE[Buttons::COUNT] = {
    BTN_LONG_TIMER_ACTIVE, BTN_SHORT_TIMER_ACTIVE, BTN_SELECT_PIN_ACTIVE,
};

// Every test starts from the same place: clock at zero, all pins released --
// which is a DIFFERENT level per button on this board.
void freshStart() {
    fakeReset();
    for (uint8_t i = 0; i < Buttons::COUNT; i++) {
        __fake_level[PINS[i]] = !ACTIVE[i];
    }
    Buttons::begin();
}

void press(Buttons::Id id)   { fakeSetPin(PINS[id], ACTIVE[id]); }
void release(Buttons::Id id) { fakeSetPin(PINS[id], !ACTIVE[id]); }

// Hold for `ms`, polling as the real loop would.
void holdFor(Buttons::Id id, uint32_t ms) {
    press(id);
    for (uint32_t t = 0; t < ms; t += BUTTON_POLL_MS) {
        fakeAdvanceMs(BUTTON_POLL_MS);
        Buttons::poll();
    }
}

// Press, hold for `ms`, release, and settle the release through debounce.
void tapFor(Buttons::Id id, uint32_t ms) {
    holdFor(id, ms);
    release(id);
    fakeAdvanceMs(BUTTON_DEBOUNCE_MS + 1);
    Buttons::poll();
}

} // namespace

TEST(buttons, a_press_is_not_down_until_it_has_debounced) {
    freshStart();
    press(Buttons::LONG_TIMER);
    Buttons::poll();
    ASSERT_TRUE(!Buttons::isDown(Buttons::LONG_TIMER));

    fakeAdvanceMs(BUTTON_DEBOUNCE_MS + 1);
    Buttons::poll();
    ASSERT_TRUE(Buttons::isDown(Buttons::LONG_TIMER));
}

TEST(buttons, a_short_tap_never_fires_a_hold) {
    // THE POINT OF THE WHOLE MODULE. A button brushed against a sleeve is a
    // tap, and a tap must not restart a play clock.
    freshStart();
    tapFor(Buttons::LONG_TIMER, TIMER_HOLD_MS / 2);

    ASSERT_TRUE(!Buttons::heldFor(Buttons::LONG_TIMER, TIMER_HOLD_MS));
    ASSERT_TRUE(!Buttons::isDown(Buttons::LONG_TIMER));
}

TEST(buttons, a_hold_fires_exactly_once_however_long_it_is_held) {
    freshStart();
    holdFor(Buttons::LONG_TIMER, TIMER_HOLD_MS + BUTTON_DEBOUNCE_MS + 40);

    ASSERT_TRUE(Buttons::heldFor(Buttons::LONG_TIMER, TIMER_HOLD_MS));
    // Still down, still past the threshold, and it must stay quiet.
    for (int i = 0; i < 10; i++) {
        fakeAdvanceMs(200);
        Buttons::poll();
        ASSERT_TRUE(!Buttons::heldFor(Buttons::LONG_TIMER, TIMER_HOLD_MS));
    }
}

TEST(buttons, releasing_and_pressing_again_arms_a_second_hold) {
    freshStart();
    holdFor(Buttons::LONG_TIMER, TIMER_HOLD_MS + 60);
    ASSERT_TRUE(Buttons::heldFor(Buttons::LONG_TIMER, TIMER_HOLD_MS));

    release(Buttons::LONG_TIMER);
    fakeAdvanceMs(BUTTON_DEBOUNCE_MS + 1);
    Buttons::poll();

    holdFor(Buttons::LONG_TIMER, TIMER_HOLD_MS + 60);
    ASSERT_TRUE(Buttons::heldFor(Buttons::LONG_TIMER, TIMER_HOLD_MS));
}

TEST(buttons, a_bounce_restarts_the_debounce_window) {
    // A contact that chatters for 20 ms and then settles has settled at the
    // LAST edge, not the first. If the window ran from the first edge the
    // button would read as down while it was still rattling.
    freshStart();
    press(Buttons::SELECT);
    fakeAdvanceMs(10);
    release(Buttons::SELECT);
    fakeAdvanceMs(10);
    press(Buttons::SELECT);
    Buttons::poll();
    ASSERT_TRUE(!Buttons::isDown(Buttons::SELECT));

    fakeAdvanceMs(BUTTON_DEBOUNCE_MS + 1);
    Buttons::poll();
    ASSERT_TRUE(Buttons::isDown(Buttons::SELECT));
}

TEST(buttons, the_hold_is_measured_from_contact_not_from_the_next_poll) {
    // The reason edges are timestamped in the handler at all. A press that
    // lands at t=0 and is not polled until t=600 has been held for 600 ms,
    // and one poll must be enough to know that.
    freshStart();
    press(Buttons::SHORT_TIMER);
    fakeAdvanceMs(TIMER_HOLD_MS + 100);   // the sketch was busy elsewhere
    Buttons::poll();

    ASSERT_TRUE(Buttons::isDown(Buttons::SHORT_TIMER));
    ASSERT_TRUE(Buttons::heldFor(Buttons::SHORT_TIMER, TIMER_HOLD_MS));
}

TEST(buttons, a_lost_edge_costs_one_poll_not_a_deaf_button) {
    // An edge that never reached the handler -- masked interrupts, or a level
    // that changed during sleep. poll() samples the pin as well as reading the
    // handler's record, so the button recovers.
    freshStart();
    fakeSetPinSilently(PINS[Buttons::SELECT], ACTIVE[Buttons::SELECT]);
    Buttons::poll();                       // notices by sampling
    fakeAdvanceMs(BUTTON_DEBOUNCE_MS + 1);
    Buttons::poll();
    ASSERT_TRUE(Buttons::isDown(Buttons::SELECT));
}

TEST(buttons, resync_suppresses_a_hold_for_a_button_already_down) {
    // The button that woke the watch is still down when the sketch comes
    // back. It must not read as a fresh press, or waking would immediately
    // start acting on it.
    freshStart();
    press(Buttons::SELECT);
    fakeAdvanceMs(BUTTON_DEBOUNCE_MS + 1);
    Buttons::poll();

    Buttons::resync();
    fakeAdvanceMs(SLEEP_HOLD_MS + 100);
    Buttons::poll();
    ASSERT_TRUE(Buttons::isDown(Buttons::SELECT));
    ASSERT_TRUE(!Buttons::heldFor(Buttons::SELECT, SLEEP_HOLD_MS));
}

// --- releasedAfter -----------------------------------------------------------
// The primitive that lets one button carry two meanings by hold length. Its
// windows are half-open -- [minMs, maxMs) -- and a report that matches no
// caller's window must survive for one that does.

TEST(buttons, a_release_reports_once_into_the_window_it_matches) {
    freshStart();
    tapFor(Buttons::SELECT, TIMER_HOLD_MS + 100);   // between menu and sleep

    // The tap window does not want it, and must not consume it.
    ASSERT_TRUE(!Buttons::releasedAfter(Buttons::SELECT, 0, TIMER_HOLD_MS));
    // The menu window does.
    ASSERT_TRUE(Buttons::releasedAfter(Buttons::SELECT, TIMER_HOLD_MS,
                                       SLEEP_HOLD_MS));
    // And exactly once.
    ASSERT_TRUE(!Buttons::releasedAfter(Buttons::SELECT, TIMER_HOLD_MS,
                                        SLEEP_HOLD_MS));
}

TEST(buttons, a_short_tap_reports_into_the_tap_window_only) {
    freshStart();
    tapFor(Buttons::SELECT, TIMER_HOLD_MS / 2);

    ASSERT_TRUE(!Buttons::releasedAfter(Buttons::SELECT, TIMER_HOLD_MS,
                                        SLEEP_HOLD_MS));
    ASSERT_TRUE(Buttons::releasedAfter(Buttons::SELECT, 0, TIMER_HOLD_MS));
}

TEST(buttons, a_new_press_clears_a_stale_release_report) {
    freshStart();
    tapFor(Buttons::SELECT, TIMER_HOLD_MS / 2);
    // Nobody consumed the tap report. A fresh press must supersede it...
    holdFor(Buttons::SELECT, TIMER_HOLD_MS + 100);
    ASSERT_TRUE(!Buttons::releasedAfter(Buttons::SELECT, 0, TIMER_HOLD_MS));
    // ...and its own release then reports its own duration.
    release(Buttons::SELECT);
    fakeAdvanceMs(BUTTON_DEBOUNCE_MS + 1);
    Buttons::poll();
    ASSERT_TRUE(Buttons::releasedAfter(Buttons::SELECT, TIMER_HOLD_MS,
                                       SLEEP_HOLD_MS));
}

TEST(buttons, resync_clears_a_pending_release_report) {
    // The sleep loop swallows presses on its own; a release it saw must not
    // surface to the main loop after the wake as a stale menu request.
    freshStart();
    tapFor(Buttons::SELECT, TIMER_HOLD_MS + 100);
    Buttons::resync();
    ASSERT_TRUE(!Buttons::releasedAfter(Buttons::SELECT, 0, SLEEP_HOLD_MS));
}

// --- The wake path -----------------------------------------------------------
// Suppressing the hold on resync() is right everywhere except the ONE place
// that is waiting for exactly that press: main.cpp's enterSleep() has to
// decide whether the button that woke the watch is being HELD or was merely
// brushed. It cannot ask heldFor(), because resync() latched the hold -- so
// it times the debounced level itself. These tests pin the TECHNIQUE and the
// trap; the copy of it lives in main.cpp, which needs the panel and Wire and
// so cannot be reached from a host. Change one and read the other.

static uint32_t wakeHoldMs() {
    const uint32_t noticed = millis();
    while (Buttons::isDown(Buttons::SELECT)) {
        if ((uint32_t)(millis() - noticed) >= SLEEP_HOLD_MS) {
            break;
        }
        fakeAdvanceMs(BUTTON_POLL_MS);
        Buttons::poll();
    }
    return (uint32_t)(millis() - noticed);
}

// Put the debouncer where it is the instant light sleep returns: the wake
// button is down, and resync() has just run.
static void afterWake() {
    freshStart();
    press(Buttons::SELECT);
    fakeAdvanceMs(BUTTON_DEBOUNCE_MS + 1);
    Buttons::poll();
    Buttons::resync();
}

TEST(buttons, a_wake_hold_is_timed_off_the_level_not_off_heldFor) {
    afterWake();
    const uint32_t held = wakeHoldMs();
    ASSERT_TRUE(Buttons::isDown(Buttons::SELECT));
    ASSERT_TRUE(held >= SLEEP_HOLD_MS);
}

TEST(buttons, a_tap_that_wakes_the_core_does_not_wake_the_watch) {
    // Any of the three buttons wakes the core -- armSleepWake() configures
    // all of them. A sleeve in a bag should cost one wake and nothing else.
    afterWake();
    fakeAdvanceMs(BUTTON_POLL_MS);
    release(Buttons::SELECT);
    fakeAdvanceMs(BUTTON_DEBOUNCE_MS + 1);
    Buttons::poll();

    const uint32_t held = wakeHoldMs();
    ASSERT_TRUE(!Buttons::isDown(Buttons::SELECT));
    ASSERT_TRUE(held < SLEEP_HOLD_MS);
}

TEST(buttons, a_wake_hold_is_measured_from_the_wake_not_from_contact) {
    // The press began before the sleep. Crediting the time already spent
    // holding would let a press made hours ago satisfy the hold the instant
    // the watch woke.
    afterWake();
    // Pretend a long time passed while down -- as it would have, asleep.
    fakeAdvanceMs(SLEEP_HOLD_MS * 10);
    Buttons::poll();
    Buttons::resync();

    const uint32_t held = wakeHoldMs();
    ASSERT_TRUE(held >= SLEEP_HOLD_MS);
}

TEST(buttons, buttons_do_not_interfere_with_each_other) {
    freshStart();
    holdFor(Buttons::LONG_TIMER, TIMER_HOLD_MS + 60);
    ASSERT_TRUE(Buttons::heldFor(Buttons::LONG_TIMER, TIMER_HOLD_MS));
    ASSERT_TRUE(!Buttons::isDown(Buttons::SHORT_TIMER));
    ASSERT_TRUE(!Buttons::heldFor(Buttons::SHORT_TIMER, TIMER_HOLD_MS));
    ASSERT_TRUE(Buttons::anyDown());
}

TEST(buttons, mixed_active_levels_do_not_cross_wires) {
    // SELECT presses LOW where the right-hand pair press HIGH. Driving every
    // pin to SELECT's pressed level must read as exactly one press -- a
    // debouncer that compared against a single BTN_PRESSED would see three.
    freshStart();
    for (uint8_t i = 0; i < Buttons::COUNT; i++) {
        fakeSetPin(PINS[i], BTN_SELECT_PIN_ACTIVE);
    }
    fakeAdvanceMs(BUTTON_DEBOUNCE_MS + 1);
    Buttons::poll();
    ASSERT_TRUE(Buttons::isDown(Buttons::SELECT));
    ASSERT_TRUE(!Buttons::isDown(Buttons::LONG_TIMER));
    ASSERT_TRUE(!Buttons::isDown(Buttons::SHORT_TIMER));
}

TEST(buttons, waitForRelease_gives_up_on_a_jammed_button) {
    // Without the timeout the watch would refuse to sleep for as long as a
    // button was stuck, which is the worst possible response to a mechanical
    // fault.
    freshStart();
    holdFor(Buttons::SELECT, BUTTON_DEBOUNCE_MS + 40);
    const uint32_t before = millis();
    Buttons::waitForRelease();
    const uint32_t waited = millis() - before;

    ASSERT_TRUE(Buttons::isDown(Buttons::SELECT));   // never released
    ASSERT_TRUE(waited >= BUTTON_RELEASE_TIMEOUT_MS);
    ASSERT_TRUE(waited < BUTTON_RELEASE_TIMEOUT_MS + 200);
}

// --- The role map, against the board ----------------------------------------
// board.h records each button's quadrant from the OSW's own placement. These
// are what catch a role landing on the wrong corner -- a change that breaks
// nothing electrical and makes the watch open the menu when you meant to
// start the short clock.

TEST(buttons, the_long_clock_is_the_top_right_button) {
    ASSERT_EQ(BTN_LONG_TIMER_PIN, PIN_BTN_UP);
    ASSERT_EQ(BTN_UP_IS_TOP, 1);
    ASSERT_EQ(BTN_UP_IS_RIGHT, 1);
}

TEST(buttons, the_short_clock_is_the_bottom_right_button) {
    ASSERT_EQ(BTN_SHORT_TIMER_PIN, PIN_BTN_DOWN);
    ASSERT_EQ(BTN_DOWN_IS_TOP, 0);
    ASSERT_EQ(BTN_DOWN_IS_RIGHT, 1);
}

TEST(buttons, menu_clear_and_sleep_are_the_bottom_left_button) {
    ASSERT_EQ(BTN_SELECT_PIN, PIN_BTN_SELECT);
    ASSERT_EQ(BTN_SELECT_IS_TOP, 0);
    ASSERT_EQ(BTN_SELECT_IS_RIGHT, 0);
}

TEST(buttons, the_three_roles_cover_three_distinct_corners) {
    // And none of them claims the top left, which is the hardware RESET --
    // wired to the ESP32's EN line, not to any GPIO. See board.h.
    const int quad[3][2] = {
        {BTN_UP_IS_TOP,     BTN_UP_IS_RIGHT},
        {BTN_DOWN_IS_TOP,   BTN_DOWN_IS_RIGHT},
        {BTN_SELECT_IS_TOP, BTN_SELECT_IS_RIGHT},
    };
    for (int i = 0; i < 3; i++) {
        ASSERT_TRUE(!(quad[i][0] == 1 && quad[i][1] == 0));   // not top-left
        for (int j = i + 1; j < 3; j++) {
            ASSERT_TRUE(quad[i][0] != quad[j][0] || quad[i][1] != quad[j][1]);
        }
    }
}

// The two hold thresholds that share the bottom-left button have to be far
// enough apart that a human can tell them apart by feel, and the menu window
// [TIMER_HOLD_MS, SLEEP_HOLD_MS) has to be non-empty or the menu becomes
// unreachable.
TEST(buttons, the_shared_buttons_two_thresholds_are_unmistakable) {
    ASSERT_TRUE(TIMER_HOLD_MS < SLEEP_HOLD_MS);
    ASSERT_TRUE(SLEEP_HOLD_MS - TIMER_HOLD_MS >= 1000);
    ASSERT_TRUE(MENU_BACK_HOLD_MS >= TIMER_HOLD_MS);
}

int main() { return runAllTests(); }
