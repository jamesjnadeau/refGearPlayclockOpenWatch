// ---------------------------------------------------------------------------
// refGear Playclock, for the Open-Smartwatch Light -- a purpose built play
// clock for officiating.
//
//   top right     hold -> start / reset the long clock  (40s by default)
//   bottom right  hold -> start / reset the short clock (25s by default)
//   bottom left   hold, release -> on the ready screen, open the settings
//                   menu; otherwise clear the clock and go back to it
//   bottom left   keep holding (~5s) -> low power mode; hold again to wake
//
// A short tap never does anything. Every tunable number lives in settings.h.
//
// ---------------------------------------------------------------------------
// THIS IS A PORT OF refGearPlayclockWatch, WHICH WAS ITSELF A REBUILD of the
// ESP32-based watchy-ref-counter -- so this firmware is an ESP32 firmware
// again, and it deliberately does NOT go back to how the original did things.
//
// The original used DEEP sleep: the chip REBOOTS on every wake, so a third of
// its sketch was machinery to work out why it had woken and reconstruct its
// state. The STM32 rebuild slept in Stop2, which retains SRAM, and deleted
// all of that; this port keeps the deletion by using the ESP32's LIGHT sleep,
// which also retains SRAM. This is a conventional loop() that sleeps between
// events with all its state intact -- including the framebuffer -- and
// nothing is reconstructed because nothing is lost. See LowPower.h for the
// milliamp that choice costs and why it is the right trade.
//
// TWO MORE THINGS THE PARENT HAD THAT THIS DOES NOT.
//
//   THE FOURTH BUTTON. The parent gave sleep its own top-left button. This
//   platform has three, so sleep is the long end of the bottom-left hold and
//   menu/clear is its short end -- see loop() and settings.h's SLEEP_HOLD_MS.
//
//   THE VCOM KEEP-ALIVE. The parent's Sharp panel demanded a polarity
//   inversion every second, threaded through every quiet path in the sketch.
//   The GC9A01 demands nothing while it sits still, so present() collapses to
//   "repaint when something changed" and the quiet paths do exactly that:
//   nothing.
// ---------------------------------------------------------------------------

#include <Arduino.h>
#include <Wire.h>

#include "BattGuard.h"
#include "Buttons.h"
#include "Buzzer.h"
#include "LowPower.h"
#include "PlayClock.h"
#include "RefDisplay.h"
#include "RefMenu.h"
#include "RefRtc.h"
#include "RefSport.h"
#include "RefStore.h"
#include "board.h"
#include "log.h"
#include "settings.h"

static RefRtc    rtc;
static PlayClock clock_;

// Cached wall clock. The RTC is read over I2C at most once a second rather
// than every loop, and the screen is only repainted when the minute changes.
static uint8_t  clockHour   = 0;
static uint8_t  clockMinute = 0;
static bool     clockValid  = false;
static uint32_t clockReadAt = 0;

// The low-voltage cutoff. Light sleep retains SRAM, so this state survives a
// sleep -- including the "this ADC has been seen to read a healthy cell"
// flag, which is the whole reason the guard is safe to act on. See BattGuard.h.
static BattGuard::State battGuard;
static uint32_t         battReadAt = 0;
static bool             battUnreadableSaid = false;

// The pins this sketch reads directly. The buttons are Buttons::begin()'s and
// the display pins are RefPanel's; what is left is the power mux status, the
// battery divider, and the RTC's interrupt line -- which this firmware never
// programs, and parks as an input so nothing else claims it.
static void configurePins() {
  pinMode(PIN_STAT_PWR, INPUT);
  pinMode(PIN_RTC_INT,  INPUT);   // externally pulled up on the OSW board
  pinMode(PIN_BATT_ADC, INPUT);
}

// Everything the screen needs, assembled from the clock and the loaded preset.
static View currentView() {
  const RefSport::Preset p = RefSport::active();
  View v = {};
  v.state        = clock_.state();
  v.secondsLeft  = clock_.secondsLeft();
  v.durationSec  = clock_.durationSec();
  v.idleLongSec  = p.longSeconds;
  v.idleShortSec = p.shortSeconds;
  v.hour         = clockHour;
  v.minute       = clockMinute;
  v.clockValid   = clockValid;
  return v;
}

// Re-read the RTC at most once a second. Returns true when the displayed
// minute changed, which is the only time the screen needs repainting for it.
static bool refreshClock() {
  const uint32_t now = millis();
  if (clockValid && (uint32_t)(now - clockReadAt) < 1000) {
    return false;
  }
  clockReadAt = now;

  struct tm t;
  uint8_t h = clockHour, m = clockMinute;
  // THE HEADER NEVER SHOWS A PLAUSIBLE WRONG TIME. An RTC whose oscillator
  // has stopped since the time was last set holds numbers, and they mean
  // nothing; the display draws --:-- instead.
  const bool ok = rtc.present() && rtc.timeIsValid() && rtc.read(t);
  if (ok) {
    h = (uint8_t)t.tm_hour;
    m = (uint8_t)t.tm_min;
  }
  const bool changed =
      (ok != clockValid) || (ok && (h != clockHour || m != clockMinute));
  clockValid  = ok;
  clockHour   = h;
  clockMinute = m;
  return changed;
}

// Push a frame when something changed. When nothing changed, nothing: this
// panel owes the firmware no keep-alive.
static void present(bool repaint) {
  if (repaint) {
    RefDisplay::render(currentView());
  }
}

static void enterIdle() {
  clock_.reset();
  present(true);
}

// What the motor says about the second that just passed -- on editions that
// have one. On the Light this whole path is a no-op.
static void buzz(MarkPlan plan) {
  switch (plan.mark) {
  case MARK_TICK:   Buzzer::play(Buzzer::TICK); break;
  case MARK_EXPIRE: Buzzer::play(Buzzer::EXPIRE); break;
  case MARK_WARN:   Buzzer::play(Buzzer::WARN, plan.count, HAPTIC_GAP_MS);
                    break;
  default: break;
  }
}

static void startTimer(uint16_t seconds) {
  // Take the timestamp FIRST: the clock starts when the hold registered, not
  // after the confirmation buzz and the frame have finished. PlayClock never
  // reads a clock of its own for exactly this reason.
  const uint32_t now = millis();
  clock_.start(RefSport::active(), seconds, now);

  Buzzer::play(Buzzer::TICK);
  present(true);
}

// Abandon whatever is on the clock and go back to the ready screen.
static void clearToIdle() {
  Buzzer::play(Buzzer::TICK);
  enterIdle();
}

// The bottom-left button's short hold does one of two things. On the ready
// screen there is no clock to clear, so it opens the settings menu instead;
// anywhere else it clears back to the ready screen.
static void bottomLeftHold() {
  if (clock_.state() != STATE_IDLE) {
    clearToIdle();
    return;
  }
  Buzzer::play(Buzzer::TICK);
  RefMenu::open(rtc);
  clockValid = false;     // the menu may have set the time
  refreshClock();
  enterIdle();
}

// Low power mode.
//
// The order below is the whole function, and every line of it is load-bearing.
//
//   THE MOTOR STOPS FIRST. A buzz left running across a sleep is a motor
//   running until the cell gives out -- the pattern player runs behind
//   loop()'s back and would not miss us.
//
//   THE PANEL GOES DARK AT ONCE AND IS NEVER WRITTEN TO AGAIN. Backlight off
//   and the controller to sleep, with the framebuffer intact. There is no
//   sleeping screen: a dark panel IS the indication that the watch is asleep,
//   and it appears the instant the hold registers rather than after a frame.
//
//   THE TIME KEEPS ITSELF. The DS3231 runs regardless of what the ESP32
//   does, so nothing here has to maintain a clock across the sleep.
//
//   THE BUTTONS ARE RELEASED BEFORE SLEEPING, or the release edge of the very
//   press that asked for sleep wakes us straight back up.
//
//   AND RE-SEEDED AFTER, because the press that woke us is timestamped from
//   its real moment of contact -- which was before the wake -- so the main
//   loop would otherwise see a hold that had long since passed its threshold
//   and act on it at once.
static void enterSleep() {
  Buzzer::off();

  RefDisplay::blank();

  Buttons::waitForRelease();
  Buttons::armSleepWake();

  // Wake on any button's level -- armSleepWake() configured all three -- but
  // only a HOLD on the bottom-left one should wake the WATCH. Anything else
  // goes straight back to sleep, which keeps "a short tap is never an action"
  // true across a wake as well: a button pressed by a sleeve in a bag costs
  // one wake and a few hundred microseconds.
  //
  // *** THE HOLD IS TIMED HERE RATHER THAN THROUGH Buttons::heldFor(). ***
  // resync() deliberately sets holdFired for any button that is already down
  // -- everywhere else that is exactly right: a button still held from before
  // must not read as a fresh press. Here it is the press we are waiting for,
  // so the hold is measured directly off the debounced level, from the moment
  // the watch noticed it.
  //
  // Measuring from "noticed" rather than from contact also asks for the full
  // hold AFTER the wake, which is the behaviour a user expects from "hold
  // again to wake" and is more forgiving than crediting a press that began
  // while the watch was asleep.
  for (;;) {
    LowPower::stopUntilInterrupt();
    Buttons::resync();

    bool woken = false;
    const uint32_t noticed = millis();
    while (Buttons::isDown(Buttons::SELECT)) {
      if ((uint32_t)(millis() - noticed) >= SLEEP_HOLD_MS) {
        woken = true;
        break;
      }
      delay(BUTTON_POLL_MS);
      Buttons::poll();
    }
    if (woken) {
      break;
    }
    Buttons::waitForRelease();
  }

  Buttons::disarmSleepWake();
  Buttons::waitForRelease();
  Buttons::resync();

  RefDisplay::unblank();
  // The RTC kept time throughout, but this firmware's cached copy is however
  // old the sleep was, so it is thrown away rather than shown for a second.
  clockValid = false;
  refreshClock();
  enterIdle();
}

// THE LAST HALF VOLT ABOVE A LIPO'S PROTECTION CUT IS FIRMWARE'S TO DEFEND.
// BattGuard holds the whole decision and the whole argument for why it is
// safe to act on an ADC nobody has measured; this is only the part that needs
// a clock, a pin and a panel.
//
// SAMPLED, NOT POLLED. Every BATT_SAMPLE_MS rather than every 20 ms pass:
// the conversion is not free, a battery does not move in five seconds, and
// the guard wants its samples spread out rather than bunched.
//
// Returns true if it slept, so loop() can give up this pass exactly as it
// does for the sleep hold rather than tick a clock that enterSleep() has
// already returned to idle.
static bool checkBattery() {
  const uint32_t now = millis();
  if (battReadAt != 0 && (uint32_t)(now - battReadAt) < BATT_SAMPLE_MS) {
    return false;
  }
  battReadAt = now;

  const bool charging = (digitalRead(PIN_STAT_PWR) == CHRG_ACTIVE);
  const BattGuard::Level level =
      BattGuard::update(battGuard, RefDisplay::batteryVolts(), charging);

  if (level == BattGuard::BATT_UNREADABLE) {
    // Not an emergency and not the battery -- at this reading nothing would
    // be running. Said once, because on a board whose divider reads that low
    // it would otherwise be said every five seconds.
    if (!battUnreadableSaid) {
      battUnreadableSaid = true;
      LOG("BATT: reading is below what the board could run at. The scale");
      LOG("      or the ADC path is wrong, not the cell. See board.h --");
      LOG("      BATT_SCALE is a one-point calibration; re-run its recipe.");
    }
    return false;
  }

  if (level != BattGuard::BATT_CUTOFF) {
    return false;
  }

  // THIS SLEEPS THE WATCH IN ANY STATE, A RUNNING CLOCK INCLUDED. Stopping an
  // official's play clock mid-down is bad; running the cell under its rating
  // is worse, and by the time this fires the gauge has been reading empty
  // since 3.40 V and the reading has been under 3.20 V for BATT_LOW_SAMPLES
  // samples in a row with nothing plugged in.
  LOG("BATT: below the cutoff and not charging -- sleeping.");
  Buzzer::off();
  enterSleep();

  // Back from the hold that woke us. The cell recovers some voltage once the
  // load comes off, and the person holding the watch has just asked for it,
  // so the run starts again from zero rather than re-cutting on the next
  // sample. `armed` is kept: it is evidence about the ADC, not about the cell.
  battGuard.lowSamples = 0;
  battReadAt = millis();
  return true;
}

static void tickRunning() {
  if (!clock_.tick(millis())) {
    return;   // nothing changed; nothing owed
  }
  // Buzz before redrawing: the buzz is the cue an official acts on and the
  // frame is not, so the frame is the one that can wait its ~35 ms.
  buzz(clock_.mark());
  present(true);
}

static void idleTick() {
  const bool busy = Buttons::anyDown();

  if (clock_.state() == STATE_EXPIRED) {
    // Hold 00 on screen a moment, then fall back to the ready screen. Waiting
    // for the buttons to be clear keeps the change from swallowing a hold that
    // is already under way.
    if (!busy && clock_.expiredHoldDone(millis())) {
      enterIdle();
    }
    return;
  }

  present(refreshClock());
}

void setup() {
  LOG_BEGIN();

  LowPower::begin();
  configurePins();
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  RefStore::begin();
  RefSport::begin();
  RefDisplay::begin();
  Buttons::begin();

  LOG("refGear Playclock (Open-Smartwatch)");
  LOG("SPORT: %s%s, clocks %u/%u", RefSport::active().name,
      RefStore::loaded() ? " (stored)" : " (settings.h default)",
      (unsigned)RefSport::active().longSeconds,
      (unsigned)RefSport::active().shortSeconds);

  if (!Buzzer::begin()) {
    LOG("HAPTIC: no motor on this edition; every buzz is a no-op.");
  }

  if (!rtc.begin()) {
    LOG("RTC: NO RESPONSE on I2C. Check SDA/SCL -- an unreachable RTC is a");
    LOG("     watch with no concept of time.");
  } else if (!rtc.timeIsValid()) {
    LOG("RTC: alive, but the time is NOT SET (OSF is set). Expected on a");
    LOG("     fresh board and after a full power loss. Menu -> Set time.");
  }

  refreshClock();
  enterIdle();
}

void loop() {
  Buttons::poll();

  // Holds first. Each one returns, so a single pass never acts on two buttons
  // -- which matters most for the pair that both start a clock. The sleep
  // hold outranks everything: it is the same button as menu/clear, and by the
  // time it fires the shorter meaning has been forgone.
  if (Buttons::heldFor(Buttons::SELECT, SLEEP_HOLD_MS)) {
    enterSleep();
    return;
  }
  if (Buttons::heldFor(Buttons::LONG_TIMER, TIMER_HOLD_MS)) {
    startTimer(RefSport::active().longSeconds);
    return;
  }
  if (Buttons::heldFor(Buttons::SHORT_TIMER, TIMER_HOLD_MS)) {
    startTimer(RefSport::active().shortSeconds);
    return;
  }
  // The short end of the bottom-left hold acts on RELEASE, because until the
  // button comes up it might still be on its way to the sleep threshold.
  if (Buttons::releasedAfter(Buttons::SELECT, TIMER_HOLD_MS, SLEEP_HOLD_MS)) {
    bottomLeftHold();
    return;
  }

  // After the holds, so a deliberate press in this pass is never pre-empted
  // by a battery sample, and before the tick, so a watch that is about to be
  // put to sleep does not first paint a frame it will immediately blank.
  if (checkBattery()) {
    return;
  }

  if (clock_.state() == STATE_RUNNING) {
    tickRunning();
  } else {
    idleTick();
  }
  delay(BUTTON_POLL_MS);
}
