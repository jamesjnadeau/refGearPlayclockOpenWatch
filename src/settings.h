// Tunables. Change a value, reflash, done -- nothing else hard-codes these.
//
// PORTED FROM refGearPlayclockWatch (the STM32 + Sharp memory LCD build) onto
// the Open-Smartwatch Light platform, and adapted where the hardware differs:
//
//   THE HAPTIC SECTION SHRANK. The parent drove a DRV2605L, which plays named
//   ROM effects of fixed length. The OSW Light has no haptic driver at all --
//   editions that have a motor drive it from a bare GPIO -- so the effect
//   numbers are gone and the pulse DURATIONS are back, exactly as they were in
//   the original watchy-ref-counter this family descends from.
//
//   THE VCOM SECTION IS GONE. The GC9A01 is a conventional TFT: no polarity
//   inversion to service, nothing owed to the panel while it sits still. What
//   it has instead is a BACKLIGHT, which the Sharp panel did not.
//
//   SLEEP_HOLD_MS GREW. The parent gave sleep its own button (top left). This
//   platform has three buttons, so sleep shares the bottom-left button with
//   the menu: half a second of hold then release opens the menu (or clears a
//   running clock); holding on to SLEEP_HOLD_MS sleeps the watch. The two
//   thresholds have to be far apart to be unmistakable by feel.

#ifndef REF_SETTINGS_H
#define REF_SETTINGS_H

#include <stdint.h>

static const char REF_PLAYCLOCK_VERSION[] = "0.1";

// --- Sport preset -----------------------------------------------------------
// Which preset the watch starts on. This is only the starting point: the
// menu's "Sport" entry picks one on the watch and stores it, and from then on
// the stored value wins. Must match one of the names in RefSport.cpp, which
// are:
//
//   Football   Lacrosse   Base NCAA  Base NFHS  Soft NCAA  Soft NFHS  Custom
//
static const char DEFAULT_SPORT[] = "Football";

// Factory values for the menu's "Custom" slot, likewise overridden once "Edit
// Custom" has been used. Clocks are 1..199 seconds; the three marks are
// 0..199, where 0 means off. All three marks are in seconds *remaining*.
static const uint16_t CUSTOM_LONG_SECONDS  = 40;
static const uint16_t CUSTOM_SHORT_SECONDS = 25;
static const uint16_t CUSTOM_WARN_SECONDS  = 10;
static const uint16_t CUSTOM_WARN2_SECONDS = 0;
static const uint16_t CUSTOM_FINAL_FROM    = 5;

// --- Appearance -------------------------------------------------------------
// BLACK SCREEN, WHITE TEXT. On the parent this was a choice about a
// reflective panel; here it is both a requirement of this port (the README
// says so in as many words) and the right call for the hardware: the GC9A01
// is emissive, so a black field is the cheapest thing it can show and the
// most readable behind a round watch glass. The expired state still inverts
// the whole screen -- a fully lit panel is the "time expired" signal an
// official can see without reading anything.
static const bool DARK_MODE = true;

// 24-hour wall clock in the header. false renders 1:23 rather than 13:23.
static const bool CLOCK_24_HOUR = false;

// Backlight duty while the watch is awake, 0..255. Full bright: this watch is
// used outdoors and the backlight is off for every second the watch sleeps.
static const uint8_t BACKLIGHT_LEVEL = 255;

// --- Battery gauge ----------------------------------------------------------
// The ends of the gauge, in volts at the CELL. These are the flat part of a
// LiPo discharge curve, not its absolute limits: 4.2 V is a full charge and
// 3.4 V is where the curve turns down hard. Below 3.4 V there is very little
// energy left and the percentage falls off a cliff, which is the right
// behaviour for a gauge -- a watch that says 20% for an hour and then dies is
// worse than one that says 5%.
static const float BATT_MAX_V = 4.20f;
static const float BATT_MIN_V = 3.40f;

// --- Low-voltage cutoff -----------------------------------------------------
// Ported with its safety argument intact, because the argument matters MORE
// here: the OSW Light's battery divider is known to read far below the cell
// (the shipped OSW firmware never even converts it to volts -- it calibrates
// raw ADC counts against observed min/max instead). BattGuard only ever acts
// after this ADC has been seen to report a plainly healthy cell, so on a board
// whose divider reads low from power-on the guard warns forever and never
// sleeps the watch -- which is the bring-up signal, not a brick. See
// BattGuard.h for how these five numbers interact.
static const float   BATT_CUTOFF_V      = 3.20f;
static const float   BATT_WARN_V        = 3.40f;
static const float   BATT_ARM_V         = 3.60f;
static const float   BATT_IMPLAUSIBLE_V = 2.00f;
static const uint8_t BATT_LOW_SAMPLES   = 5;

// How often the guard reads the cell. The ADC read is not free and nothing
// about a battery changes in five seconds; five samples at this interval is
// twenty-five seconds of sustained low reading before the watch acts.
static const uint32_t BATT_SAMPLE_MS = 5000;

// --- Buttons ----------------------------------------------------------------
// The parent's values, unchanged. 30 ms of debounce against a 20 ms poll is
// the classic pairing: long enough that a tactile switch has finished
// bouncing, short enough that it is invisible to a person.
static const uint32_t BUTTON_POLL_MS     = 20;
static const uint32_t BUTTON_DEBOUNCE_MS = 30;

// Give up waiting for a stuck button after this long, so the watch can never
// be held awake by a jammed button.
static const uint32_t BUTTON_RELEASE_TIMEOUT_MS = 5000;

// How long a right-hand button must be held before it starts (or restarts)
// its clock. A SHORT TAP IS DELIBERATELY IGNORED so a bumped button during a
// game cannot reset the play clock. This is the parent's central design
// decision about buttons and it matters more on a watch worn during exercise,
// not less.
static const uint32_t TIMER_HOLD_MS = 500;

// How long the bottom-left (menu) button must be held to drop into low power
// mode.
//
// MUCH LONGER THAN THE PARENT'S 1000, because the roles moved. The parent had
// four buttons and gave sleep its own; this platform has three usable ones
// (the fourth corner is the hardware RESET -- board.h), so sleep and
// menu/clear share the bottom-left button, split by hold length: reach
// TIMER_HOLD_MS and let go for menu/clear, keep holding to here for sleep.
// Five seconds is deliberate -- long enough that nobody wanders into it on
// the way to the menu, and sleep is the one action here that is never needed
// in a hurry.
static const uint32_t SLEEP_HOLD_MS = 5000;

// --- Play clock -------------------------------------------------------------
// How the countdown announces itself: how MANY times each mark buzzes. Either
// count may be 0 to silence that mark for every preset at once. Which second
// each mark falls on is per-sport and lives in RefSport.cpp.
//
// A mark at or below a preset's final-countdown value is SWALLOWED by the
// per-second countdown, which buzzes there anyway. PlayClock's mark precedence
// is what does the swallowing and playclock_test asserts it.
static const uint8_t WARNING_BUZZ_COUNT   = 1;
static const uint8_t WARNING_BUZZ_COUNT_2 = 2;

// How long `00` stays up after a clock expires before the watch drops back to
// the ready screen. Starting a new clock during this window cancels it. The
// human reason survives every port this number has been through: an official
// who looked away at 01 needs to see the 00.
static const uint32_t EXPIRED_HOLD_MS = 3000;

// --- Menu -------------------------------------------------------------------
// Leave the menu and return to the ready screen after this long with no button
// presses, so a menu opened by accident cannot strand you mid-game.
static const uint32_t MENU_TIMEOUT_MS = 15000;

// How long the bottom-left button must be held, inside the menu, to act as
// BACK. The parent had a dedicated BACK button (top left); with three buttons
// the same physical button is tap-for-SELECT and hold-for-BACK, and this is
// the hold. Same value as TIMER_HOLD_MS so the whole watch has one meaning
// for "a held button".
static const uint32_t MENU_BACK_HOLD_MS = 500;

// How fast an editable field blinks on the set-time and edit-custom screens.
// Half a second on, half a second off.
static const uint32_t MENU_BLINK_MS = 500;

// How many characters of a menu row's LABEL are guaranteed to fit.
//
// The menu rows draw in FreeSans12pt7b starting at x = 40, and the widest
// chord available to a row near the middle of the round panel is about
// 200 px of usable run before the value column. FreeSans12pt averages a
// little under 13 px per character, so 14 characters is the same defensive
// budget the parent drew for its panel: it exists to catch a label that
// GROWS, not to prove one fits. Asserted in menu_items_test as a character
// count, which is a weak bound for a proportional face and the only one a
// host test can check.
static const uint8_t MENU_LABEL_MAX_CHARS = 14;

// --- Haptics ----------------------------------------------------------------
// A bare motor pin, driven for a number of milliseconds -- the original
// watchy-ref-counter scheme, back because the DRV2605L did not make this
// board. The OSW LIGHT HAS NO MOTOR AT ALL: Buzzer compiles to a no-op there
// (see board.h's PIN_VIB) and these numbers wait for an edition that has one.
static const uint32_t BUZZ_TICK_MS   = 60;   // a play-clock tick: short click
static const uint32_t BUZZ_WARN_MS   = 80;   // one pulse of a warning pattern
static const uint32_t BUZZ_EXPIRE_MS = 500;  // zero: a long buzz

// Gap between repeats when a pattern is played more than once.
static const uint32_t HAPTIC_GAP_MS = 120;

#endif // REF_SETTINGS_H
