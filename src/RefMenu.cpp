// ---------------------------------------------------------------------------
// The settings menu.
//
// The shape of this -- a highlighted list, UP and DOWN to move, SELECT to
// choose, BACK to leave, a set-time screen whose current field blinks --
// follows the parent, refGearPlayclockWatch, and through it watchy-ref-counter
// and the reference Watchy firmware (sqfmi/Watchy, MIT).
//
// ---------------------------------------------------------------------------
// TWO THINGS THIS PLATFORM CHANGED, AND THEY ARE NOT COSMETIC.
//
// 1. THERE IS NO BACK BUTTON. The parent had four buttons and spent its top
//    left one on BACK; this watch has three. So the bottom-left button does
//    both jobs, split by time exactly as it is on the play-clock screen: a
//    TAP is SELECT, a HOLD (MENU_BACK_HOLD_MS) is BACK. SELECT therefore
//    fires on RELEASE rather than on press -- the only way to know a press
//    was not going to become a hold is to see it end -- which costs the tap a
//    perceptible nothing and keeps every screen reachable and leavable with
//    three buttons.
//
// 2. THE PANEL IS ROUND. 240 px across, but the top and bottom rows see a
//    narrow chord of it, so every screen here keeps its dense lines near the
//    vertical middle and its single hint line small and centred at the
//    bottom. The layout constants carry the same static_assert discipline as
//    RefLayout.h, circle included.
//
// AND ONE THING THE PARENT'S PANEL DEMANDED THAT THIS ONE DOES NOT: the VCOM
// keep-alive. A settled GC9A01 wants nothing, so Screen::pace() is just the
// poll delay and a screen that has not changed pushes nothing.
// ---------------------------------------------------------------------------

#include "RefMenu.h"

#include <Fonts/FreeSans12pt7b.h>
#include <string.h>
#include <time.h>

#include "Buttons.h"
#include "Buzzer.h"
#include "RefCustomEdit.h"
#include "RefDisplay.h"
#include "RefDrift.h"
#include "RefLayout.h"
#include "RefMenuItems.h"
#include "RefPanel.h"
#include "RefSetTime.h"
#include "RefSport.h"
#include "RefStore.h"
#include "board.h"
#include "settings.h"

namespace RefMenu {
namespace {

auto &display = RefPanel::display;

// The built-in 5x7 face at two sizes. Size 2 (12 px advance, 16 tall) is the
// data face -- at this panel's dot pitch it is the same physical size the
// parent's 6x8 was. Size 1 is for the hint lines, which sit low on the
// circle where only a narrow chord survives.
const int16_t SMALL_ADV = 12;
const int16_t SMALL_H   = 16;
const int16_t HINT_ADV  = 6;
const int16_t HINT_H    = 8;

// What every snprintf in this file formats into. Generous so snprintf can
// never be the thing that truncates -- a string cut short by the buffer looks
// identical to one cut short by the panel and has a completely different fix.
const size_t LINE_BUF = 48;

// ---- where every screen puts things ---------------------------------------
// Gathered here rather than left inside the five functions, so the block of
// static_asserts at the bottom of this file can check them -- this file is
// the one part of the firmware a host test cannot reach.

// FreeSans12pt7b: about 18 px of cap above the baseline and 5 below.
constexpr int16_t BIG_ASCENT  = 18;
constexpr int16_t BIG_DESCENT = 5;

// One hint line, small and centred, along the bottom of every screen.
constexpr int16_t HINT_Y = 214;

// The list.
constexpr int16_t MENU_ROW_H   = 34;
constexpr int16_t MENU_FIRST_Y = 88;   // baseline of the first row
constexpr int16_t MENU_TEXT_X  = 40;

// Set time: two STYLE_MED pairs with a colon between them, the date beneath.
constexpr int16_t SET_PAIR_W  = STYLE_MED.w * 2 + STYLE_MED.gap;
constexpr int16_t SET_LEFT_X  = 20;
constexpr int16_t SET_RIGHT_X = SCREEN_W - SET_PAIR_W - 20;
constexpr int16_t SET_COLON_W = 8;
constexpr int16_t SET_COLON_X = (SET_LEFT_X + SET_PAIR_W + SET_RIGHT_X) / 2
                                - SET_COLON_W / 2;
constexpr int16_t SET_PAIR_Y  = 64;
constexpr int16_t SET_DATE_Y  = 170;   // baseline
constexpr int16_t SET_HINT_Y  = 188;

// The sport picker: a scrolling window over seven presets, then a rule and a
// detail block for the highlighted one.
constexpr int16_t SPORT_ROW_H   = 24;
constexpr uint8_t SPORT_VISIBLE = 5;
constexpr int16_t SPORT_FIRST_Y = 72;   // baseline of the first row
constexpr int16_t SPORT_TEXT_X  = 48;
constexpr int16_t SPORT_RULE_Y  = 180;

// Edit Custom: a title, a rule, five labelled rows.
constexpr int16_t CUSTOM_TITLE_Y = 62;
constexpr int16_t CUSTOM_RULE_Y  = 68;
constexpr int16_t CUSTOM_FIRST_Y = 94;
constexpr int16_t CUSTOM_ROW_H   = 24;
constexpr int16_t CUSTOM_TEXT_X  = 48;

void smallFont() {
  display.setFont(nullptr);
  display.setTextSize(2);
}

void hintFont() {
  display.setFont(nullptr);
  display.setTextSize(1);
}

void bigFont() {
  display.setFont(&FreeSans12pt7b);
  display.setTextSize(1);
}

void smallText(int16_t x, int16_t top, const char *s) {
  display.setCursor(x, top);
  display.print(s);
}

// Centred against the panel's vertical axis, which on a round panel is the
// only alignment that treats every line fairly. Arithmetic rather than a
// getTextBounds round trip, which the fixed-width face makes exact.
void smallTextCentred(int16_t top, const char *s) {
  const int16_t w = (int16_t)(strlen(s) * SMALL_ADV);
  smallText((int16_t)((SCREEN_W - w) / 2), top, s);
}

void hintText(int16_t top, const char *s) {
  const int16_t w = (int16_t)(strlen(s) * HINT_ADV);
  display.setCursor((int16_t)((SCREEN_W - w) / 2), top);
  display.print(s);
}

void bigText(int16_t x, int16_t baseline, const char *s) {
  display.setCursor(x, baseline);
  display.print(s);
}

// FreeSans is proportional, so this one does need to measure.
void bigTextRight(int16_t baseline, const char *s) {
  int16_t  x1, y1;
  uint16_t w, h;
  display.getTextBounds(s, 0, baseline, &x1, &y1, &w, &h);
  display.setCursor(SCREEN_W - CUSTOM_TEXT_X - (int16_t)w - x1, baseline);
  display.print(s);
}

// ---- input -----------------------------------------------------------------
// BUTTON ROLES IN THE MENU FOLLOW THE HANDS. The right column moves you up
// and down; the single left button moves you in (tap) and out (hold):
//
//                                UP     (top right, tap)
//     SELECT tap / BACK hold
//     (bottom left)              DOWN   (bottom right, tap)
//
// SELECT is the same button that opened the menu, which is what a thumb
// already resting there expects.
const Buttons::Id BTN_UP   = Buttons::LONG_TIMER;
const Buttons::Id BTN_DOWN = Buttons::SHORT_TIMER;

struct Edges {
  bool select, exit, up, down;
  bool any() const { return select || exit || up || down; }
};

bool wasDown[Buttons::COUNT];

void seedEdges() {
  Buttons::poll();
  for (uint8_t i = 0; i < Buttons::COUNT; i++) {
    wasDown[i] = Buttons::isDown((Buttons::Id)i);
  }
}

// UP and DOWN are presses, not levels, so a button held down produces exactly
// one edge and leaning on DOWN cannot walk the menu. SELECT and BACK share
// the bottom-left button: a release before MENU_BACK_HOLD_MS is a tap and
// selects; reaching MENU_BACK_HOLD_MS is a hold and goes back -- delivered
// once per press by Buttons' own latches.
Edges readEdges() {
  Buttons::poll();
  bool now[Buttons::COUNT];
  for (uint8_t i = 0; i < Buttons::COUNT; i++) {
    now[i] = Buttons::isDown((Buttons::Id)i);
  }
  Edges e;
  e.select = Buttons::releasedAfter(Buttons::SELECT, 0, MENU_BACK_HOLD_MS);
  e.exit   = Buttons::heldFor(Buttons::SELECT, MENU_BACK_HOLD_MS);
  e.up     = now[BTN_UP] && !wasDown[BTN_UP];
  e.down   = now[BTN_DOWN] && !wasDown[BTN_DOWN];
  for (uint8_t i = 0; i < Buttons::COUNT; i++) {
    wasDown[i] = now[i];
  }
  return e;
}

// ---- the pacing every modal screen shares ----------------------------------
class Screen {
public:
  Screen() {
    _lastActivity = millis();
    _blinkAt      = _lastActivity + MENU_BLINK_MS;
  }

  // False once MENU_TIMEOUT_MS has passed with nothing pressed. Every screen
  // in this file falls out of its loop on that, and every screen that could
  // commit something treats falling out as "commit nothing".
  bool alive() const {
    return (uint32_t)(millis() - _lastActivity) < MENU_TIMEOUT_MS;
  }

  Edges read() {
    const Edges e = readEdges();
    if (e.any()) {
      _lastActivity = millis();
      _dirty        = true;
    }
    if ((int32_t)(millis() - _blinkAt) >= 0) {
      _blink   = !_blink;
      _blinkAt = millis() + MENU_BLINK_MS;
      _dirty   = true;
    }
    return e;
  }

  // True while an editable field should be drawn lit.
  bool blink() const { return _blink; }

  // True when the screen's content has changed and has to be redrawn.
  bool needsPaint() const { return _dirty; }

  void painted() {
    RefDisplay::refresh();
    _dirty = false;
  }

  void pace() { delay(BUTTON_POLL_MS); }

private:
  uint32_t _lastActivity;
  uint32_t _blinkAt;
  bool     _blink = true;
  bool     _dirty = true;
};

void beginScreen() {
  display.fillScreen(THEME_BG);
  display.setTextColor(THEME_FG);
}

// ---- About -----------------------------------------------------------------
// Version, battery, the live clock, the drift reminder, and the loaded sport.
// Repainted rather than held, so the wall clock on this screen ticks and the
// battery reading is live -- which is most of what anyone opens it for.
void paintAbout(RefRtc &rtc);

void showAbout(RefRtc &rtc) {
  Screen scr;
  while (scr.alive()) {
    if (scr.read().exit) {
      return;
    }
    if (!scr.needsPaint()) {
      scr.pace();
      continue;
    }
    paintAbout(rtc);
    scr.painted();
    scr.pace();
  }
}

void paintAbout(RefRtc &rtc) {
  beginScreen();
  smallFont();

  // Centred lines, the longest kept near the vertical middle where the round
  // panel is widest. The line step leaves a blank pixel row between glyphs.
  int16_t y = 56;
  const int16_t LINE = 20;
  char line[LINE_BUF];

  smallTextCentred(y, "refGear Playclock");
  y += LINE;

  // A number rather than the header's bar, because the question here is
  // whether the divider and BATT_DIVIDER's guess are working at all.
  const float v = RefDisplay::batteryVolts();
  snprintf(line, sizeof(line), "v%s   %d.%02dV", REF_PLAYCLOCK_VERSION, (int)v,
           (int)((v - (float)(int)v) * 100.0f + 0.5f));
  smallTextCentred(y, line);
  y += LINE;

  struct tm now;
  const bool haveTime = rtc.present() && rtc.timeIsValid() && rtc.read(now);
  if (haveTime) {
    snprintf(line, sizeof(line), "%04d/%02d/%02d %02d:%02d",
             now.tm_year + 1900, now.tm_mon + 1, now.tm_mday, now.tm_hour,
             now.tm_min);
  } else {
    snprintf(line, sizeof(line), "time NOT SET");
  }
  smallTextCentred(y, line);
  y += LINE;

  // How long since the clock was set by hand, and whether that is long enough
  // to be worth doing again.
  const time_t setAt = (time_t)RefStore::get(RefStore::KEY_CLOCK_SET_AT, 0);
  const time_t nowEpoch = haveTime ? rtc.epoch() : 0;
  RefDrift::describe(line, sizeof(line), setAt, nowEpoch);
  smallTextCentred(y, line);
  y += LINE;
  if (RefDrift::shouldRemind(setAt, nowEpoch)) {
    snprintf(line, sizeof(line), "~%us adrift: reset",
             (unsigned)RefDrift::worstCaseDriftSeconds(setAt, nowEpoch));
    smallTextCentred(y, line);
  }
  y += LINE;

  snprintf(line, sizeof(line), "RTC: %s",
           !rtc.present() ? "NOT FOUND"
                          : (rtc.timeIsValid() ? "DS3231 ok" : "DS3231 OSF"));
  smallTextCentred(y, line);
  y += LINE;

  const RefSport::Preset p = RefSport::active();
  snprintf(line, sizeof(line), "Sport: %s", p.name);
  smallTextCentred(y, line);
  y += LINE + 2;

  // All five numbers, in the hint face -- this low on the circle only a
  // narrow chord is visible. w = the two warning marks, f = the final
  // countdown; 0 means off.
  hintFont();
  snprintf(line, sizeof(line), "%u/%u  w%u/%u f%u", (unsigned)p.longSeconds,
           (unsigned)p.shortSeconds, (unsigned)p.warnAtSeconds,
           (unsigned)p.warn2AtSeconds, (unsigned)p.finalCountdownFrom);
  hintText(y, line);

  hintText(HINT_Y, "hold to exit");
}

// ---- Set time --------------------------------------------------------------
// The state machine is RefSetTime's; this is only its face. A TAP advances
// and commits past the last field, a HOLD steps back, UP and DOWN change the
// value under the cursor, and the field being edited blinks -- which is what
// tells you where you are.
//
// THERE IS NO CANCEL AND A TIMEOUT IS NOT ONE. Falling out of the loop leaves
// committed() false, so a half-finished edit never reaches the chip. That
// matters more here than anywhere else in the firmware: a clock left half set
// is a watch showing a plausible wrong time.
void setTime(RefRtc &rtc) {
  RefSetTime edit;
  struct tm current;
  if (rtc.present() && rtc.timeIsValid() && rtc.read(current)) {
    edit.begin(current.tm_year + 1900, (uint8_t)(current.tm_mon + 1),
               (uint8_t)current.tm_mday, (uint8_t)current.tm_hour,
               (uint8_t)current.tm_min);
  } else {
    // Nothing sensible on the chip, so start from the bottom of the range
    // RefSetTime will accept rather than from whatever it powered up holding.
    edit.begin(RefSetTime::YEAR_MIN, 1, 1, 0, 0);
  }

  Screen s;
  while (s.alive() && !edit.committed()) {
    const Edges e = s.read();
    if (e.select) {
      edit.advance();
    }
    if (e.exit) {
      // A cursor move, not a cancel. Inert on the first field, deliberately:
      // a BACK that could also abandon the screen would abandon it by
      // accident, and a half-set clock is exactly what must not happen.
      edit.back();
    }
    if (e.up) {
      edit.up();
    }
    if (e.down) {
      edit.down();
    }

    if (s.needsPaint()) {
      beginScreen();

      const RefSetTime::Field f = edit.field();
      RefDisplay::drawDigitPair(SET_LEFT_X, SET_PAIR_Y, (uint8_t)edit.hour(),
                                f != RefSetTime::FIELD_HOUR || s.blink());
      RefDisplay::drawDigitPair(SET_RIGHT_X, SET_PAIR_Y, (uint8_t)edit.minute(),
                                f != RefSetTime::FIELD_MINUTE || s.blink());
      display.fillRect(SET_COLON_X, SET_PAIR_Y + 16, SET_COLON_W, SET_COLON_W,
                       THEME_FG);
      display.fillRect(SET_COLON_X, SET_PAIR_Y + 40, SET_COLON_W, SET_COLON_W,
                       THEME_FG);

      // The date, with the field under the cursor blanked on the off beat.
      bigFont();
      char date[16];
      snprintf(date, sizeof(date), "%04d/%02u/%02u", edit.year(),
               (unsigned)edit.month(), (unsigned)edit.day());
      // Drawn whole, then the blinking field painted over in the background
      // colour -- one string keeps the slashes and the spacing right.
      display.setTextColor(THEME_FG);
      int16_t  x1, y1;
      uint16_t w, h;
      display.getTextBounds(date, 0, SET_DATE_Y, &x1, &y1, &w, &h);
      const int16_t dateX = SCREEN_W / 2 - (int16_t)(w / 2) - x1;
      bigText(dateX, SET_DATE_Y, date);
      if (!s.blink() && f >= RefSetTime::FIELD_YEAR) {
        char part[8];
        int16_t px = dateX;
        if (f == RefSetTime::FIELD_YEAR) {
          snprintf(part, sizeof(part), "%04d", edit.year());
        } else if (f == RefSetTime::FIELD_MONTH) {
          snprintf(part, sizeof(part), "%02u", (unsigned)edit.month());
          char head[8];
          snprintf(head, sizeof(head), "%04d/", edit.year());
          display.getTextBounds(head, 0, SET_DATE_Y, &x1, &y1, &w, &h);
          px += (int16_t)w;
        } else {
          snprintf(part, sizeof(part), "%02u", (unsigned)edit.day());
          char head[12];
          snprintf(head, sizeof(head), "%04d/%02u/", edit.year(),
                   (unsigned)edit.month());
          display.getTextBounds(head, 0, SET_DATE_Y, &x1, &y1, &w, &h);
          px += (int16_t)w;
        }
        display.setTextColor(THEME_BG);
        bigText(px, SET_DATE_Y, part);
        display.setTextColor(THEME_FG);
      }

      hintFont();
      const char *what = "";
      switch (f) {
      case RefSetTime::FIELD_HOUR:   what = "hour"; break;
      case RefSetTime::FIELD_MINUTE: what = "minute"; break;
      case RefSetTime::FIELD_YEAR:   what = "year"; break;
      case RefSetTime::FIELD_MONTH:  what = "month"; break;
      default:                       what = "day"; break;
      }
      char hint[LINE_BUF];
      snprintf(hint, sizeof(hint), "%s  %u of %u", what, (unsigned)(f + 1),
               (unsigned)RefSetTime::FIELD_COUNT);
      hintText(SET_HINT_Y, hint);
      hintText(HINT_Y, "tap next  hold prev");

      s.painted();
    }
    s.pace();
  }

  if (!edit.committed()) {
    return;   // timed out part way through; the clock keeps what it had
  }

  struct tm t = {};
  t.tm_year = edit.year() - 1900;
  t.tm_mon  = (int)edit.month() - 1;
  t.tm_mday = (int)edit.day();
  t.tm_hour = (int)edit.hour();
  t.tm_min  = (int)edit.minute();
  t.tm_sec  = 0;
  if (!rtc.set(t)) {
    beginScreen();
    smallFont();
    smallTextCentred(100, "CLOCK WRITE");
    smallTextCentred(124, "FAILED");
    RefDisplay::refresh();
    delay(2000);
    return;
  }

  // Remember WHEN, so the About screen can say how far the part is specified
  // to have drifted since.
  RefStore::setAndCommit(RefStore::KEY_CLOCK_SET_AT, (uint32_t)rtc.epoch());
  Buzzer::play(Buzzer::TICK);
}

// ---- Sport picker ----------------------------------------------------------
// A TAP takes the highlighted preset, a HOLD leaves the stored one alone. The
// rows carry names only; the highlighted preset's five numbers ride in the
// detail block under the rule.
void pickSport() {
  const uint8_t total = RefSport::count();

  uint8_t index = RefSport::index();
  uint8_t top   = index < SPORT_VISIBLE ? 0 : (uint8_t)(index - SPORT_VISIBLE + 1);

  Screen s;
  while (s.alive()) {
    const Edges e = s.read();
    if (e.select) {
      RefSport::setIndex(index);
      Buzzer::play(Buzzer::TICK);
      return;
    }
    if (e.exit) {
      return;
    }
    if (e.up) {
      index = (index == 0) ? (uint8_t)(total - 1) : (uint8_t)(index - 1);
    }
    if (e.down) {
      index = (uint8_t)((index + 1) % total);
    }

    if (s.needsPaint()) {
      // Keep the highlighted row inside the visible window.
      if (index < top) {
        top = index;
      } else if (index >= top + SPORT_VISIBLE) {
        top = (uint8_t)(index - SPORT_VISIBLE + 1);
      }

      beginScreen();
      bigFont();
      for (uint8_t slot = 0; slot < SPORT_VISIBLE && top + slot < total; slot++) {
        const uint8_t   row  = (uint8_t)(top + slot);
        const int16_t   yPos = SPORT_FIRST_Y + SPORT_ROW_H * slot;
        const RefSport::Preset p = RefSport::preset(row);
        if (row == index) {
          display.fillRect(0, yPos - BIG_ASCENT, SCREEN_W, SPORT_ROW_H - 1,
                           THEME_FG);
          display.setTextColor(THEME_BG);
        } else {
          display.setTextColor(THEME_FG);
        }
        bigText(SPORT_TEXT_X, yPos, p.name);
      }

      display.setTextColor(THEME_FG);
      display.drawFastHLine(0, SPORT_RULE_Y, SCREEN_W, THEME_FG);

      const RefSport::Preset sel = RefSport::preset(index);
      hintFont();
      char line[LINE_BUF];
      snprintf(line, sizeof(line), "%s   %u/%u", sel.description,
               (unsigned)(index + 1), (unsigned)total);
      hintText(SPORT_RULE_Y + 8, line);

      // All five numbers, which is more than a row could carry. w = the two
      // warning marks, f = the final countdown; 0 means off.
      snprintf(line, sizeof(line), "%u/%u  w%u/%u f%u",
               (unsigned)sel.longSeconds, (unsigned)sel.shortSeconds,
               (unsigned)sel.warnAtSeconds, (unsigned)sel.warn2AtSeconds,
               (unsigned)sel.finalCountdownFrom);
      hintText(SPORT_RULE_Y + 20, line);

      hintText(HINT_Y, "tap pick  hold keep");
      s.painted();
    }
    s.pace();
  }
}

// ---- Edit Custom -----------------------------------------------------------
// The five numbers behind the one editable preset. The rules are
// RefCustomEdit's -- and they are not obvious rules, which is why they live
// somewhere a host test can reach them.
//
// THIS DOES NOT SELECT CUSTOM. The user picks it from the Sport screen, which
// keeps "edit" and "use" separate.
void editCustom() {
  RefCustomEdit edit;
  edit.begin(RefSport::custom());

  Screen s;
  while (s.alive() && !edit.committed()) {
    const Edges e = s.read();
    if (e.select) {
      edit.advance();
    }
    if (e.exit) {
      edit.back();
    }
    if (e.up) {
      edit.up();
    }
    if (e.down) {
      edit.down();
    }

    if (s.needsPaint()) {
      beginScreen();
      bigFont();
      display.setTextColor(THEME_FG);
      bigText(CUSTOM_TEXT_X, CUSTOM_TITLE_Y, "EDIT CUSTOM");
      display.drawFastHLine(0, CUSTOM_RULE_Y, SCREEN_W, THEME_FG);

      char num[8];
      for (uint8_t f = 0; f < RefCustomEdit::FIELD_COUNT; f++) {
        const int16_t yPos = CUSTOM_FIRST_Y + CUSTOM_ROW_H * f;
        display.setTextColor(THEME_FG);
        bigText(CUSTOM_TEXT_X, yPos, RefCustomEdit::label((RefCustomEdit::Field)f));
        snprintf(num, sizeof(num), "%u",
                 (unsigned)edit.value((RefCustomEdit::Field)f));
        const bool hidden = (f == edit.field()) && !s.blink();
        display.setTextColor(hidden ? THEME_BG : THEME_FG);
        bigTextRight(yPos, num);
      }

      display.setTextColor(THEME_FG);
      hintFont();
      hintText(HINT_Y, "tap next  hold prev");
      s.painted();
    }
    s.pace();
  }

  if (!edit.committed()) {
    return;   // a stale in-progress edit is never persisted
  }
  RefSport::setCustom(edit.value(RefCustomEdit::FIELD_LONG),
                      edit.value(RefCustomEdit::FIELD_SHORT),
                      edit.value(RefCustomEdit::FIELD_WARN),
                      edit.value(RefCustomEdit::FIELD_WARN2),
                      edit.value(RefCustomEdit::FIELD_FINAL));
  Buzzer::play(Buzzer::TICK);
}

// ---- the list --------------------------------------------------------------
// FOUR UNCONDITIONAL ROWS, so there is no scrolling window and no slot
// indirection.
void drawMenu(uint8_t item) {
  beginScreen();
  bigFont();

  char label[RefMenu::ITEM_LABEL_MAX];
  char value[RefMenu::ITEM_LABEL_MAX];
  for (uint8_t i = 0; i < ITEM_COUNT; i++) {
    const int16_t yPos = MENU_FIRST_Y + MENU_ROW_H * i;
    itemLabel(i, label, sizeof(label));
    itemValue(i, value, sizeof(value));

    if (i == item) {
      display.fillRect(0, yPos - BIG_ASCENT - 3, SCREEN_W, MENU_ROW_H - 2,
                       THEME_FG);
      display.setTextColor(THEME_BG);
    } else {
      display.setTextColor(THEME_FG);
    }
    bigFont();
    bigText(MENU_TEXT_X, yPos, label);
    if (value[0] != '\0') {
      // The value rides in the hint face, right-aligned inside the chord, so
      // the pair fits and the menu keeps doubling as the status display --
      // which sport is loaded is the thing worth checking before a game.
      hintFont();
      const int16_t w = (int16_t)(strlen(value) * HINT_ADV);
      display.setCursor(SCREEN_W - MENU_TEXT_X - w, yPos - HINT_H);
      display.print(value);
    }
  }

  display.setTextColor(THEME_FG);
  hintFont();
  hintText(HINT_Y, "tap open  hold exit");
}

// ---- what has to be true ---------------------------------------------------
// Asserts rather than a look at the render, for RefLayout.h's reason:
// Adafruit_GFX clips silently at the square edge and the bezel clips silently
// at the circle, so a row half hidden produces a screen that looks like a
// font problem. Nothing in this file has a host test behind it -- these are
// the only mechanical check the menu's geometry gets.

// The list.
static_assert(MENU_FIRST_Y - BIG_ASCENT > 0,
              "the first menu row starts above the top of the panel");
static_assert(MENU_FIRST_Y + MENU_ROW_H * (ITEM_COUNT - 1) + BIG_DESCENT
                  < HINT_Y,
              "the last menu row runs into the hint line -- ITEM_COUNT has "
              "grown past what fits without a scrolling window");
static_assert(HINT_Y + HINT_H <= SCREEN_H,
              "the hint line runs off the bottom");
static_assert(rectVisible(MENU_TEXT_X, MENU_FIRST_Y - BIG_ASCENT,
                          SCREEN_W - 2 * MENU_TEXT_X,
                          MENU_ROW_H * ITEM_COUNT),
              "the menu rows leak outside the visible circle");

// Set time. The two digit pairs and the colon share one row, and the gap
// between the pairs is the only place the colon can go.
static_assert(SET_LEFT_X + SET_PAIR_W < SET_COLON_X,
              "the colon overlaps the hours");
static_assert(SET_COLON_X + SET_COLON_W <= SET_RIGHT_X,
              "the colon overlaps the minutes");
static_assert(rectVisible(SET_LEFT_X, SET_PAIR_Y,
                          SET_RIGHT_X + SET_PAIR_W - SET_LEFT_X, STYLE_MED.h),
              "the set-time digit row leaks outside the visible circle");
static_assert(SET_PAIR_Y + STYLE_MED.h < SET_DATE_Y - BIG_ASCENT,
              "the date line runs into the digits");
static_assert(SET_DATE_Y + BIG_DESCENT < SET_HINT_Y,
              "the first hint line runs into the date");
static_assert(SET_HINT_Y + HINT_H < HINT_Y,
              "the two set-time hint lines collide");

// The sport picker.
static_assert(SPORT_FIRST_Y - BIG_ASCENT > 0,
              "the first preset row starts above the top of the panel");
static_assert(SPORT_FIRST_Y + SPORT_ROW_H * (SPORT_VISIBLE - 1) + BIG_DESCENT
                  < SPORT_RULE_Y,
              "the preset rows run past the rule");
static_assert(SPORT_RULE_Y + 20 + HINT_H < HINT_Y,
              "the detail block runs into the hint line");
static_assert(rectVisible(SPORT_TEXT_X, SPORT_FIRST_Y - BIG_ASCENT,
                          120, SPORT_ROW_H * SPORT_VISIBLE),
              "the preset rows leak outside the visible circle");

// Edit Custom.
static_assert(CUSTOM_TITLE_Y < CUSTOM_RULE_Y,
              "the title sits below its own rule");
static_assert(CUSTOM_RULE_Y < CUSTOM_FIRST_Y - BIG_ASCENT,
              "the first field row runs into the rule");
static_assert(CUSTOM_FIRST_Y
                      + CUSTOM_ROW_H * (RefCustomEdit::FIELD_COUNT - 1)
                      + BIG_DESCENT
                  < HINT_Y,
              "the last field row runs into the hint line");
static_assert(rectVisible(CUSTOM_TEXT_X, CUSTOM_FIRST_Y - BIG_ASCENT,
                          SCREEN_W - 2 * CUSTOM_TEXT_X,
                          CUSTOM_ROW_H * RefCustomEdit::FIELD_COUNT),
              "the custom rows leak outside the visible circle");

} // namespace

void open(RefRtc &rtc) {
  // The menu opens on the RELEASE of the bottom-left hold, so the button is
  // normally already up; the wait catches the other path (a jammed or
  // still-settling button) and costs nothing on the common one.
  Buttons::waitForRelease();
  seedEdges();

  uint8_t item = ITEM_ABOUT;

  // Screen starts dirty, so the first pass of the loop is what paints the
  // list. Painting here as well would push two identical frames.
  Screen s;
  while (s.alive()) {
    const Edges e = s.read();

    if (e.exit) {
      break;   // out of the menu entirely
    }
    if (e.select) {
      switch (item) {
      case ITEM_ABOUT:       showAbout(rtc); break;
      case ITEM_SET_TIME:    setTime(rtc); break;
      case ITEM_SPORT:       pickSport(); break;
      case ITEM_EDIT_CUSTOM: editCustom(); break;
      default: break;
      }
      // Every action can change what a row says -- the sport row certainly
      // does -- so a fresh Screen starts dirty and the loop repaints.
      seedEdges();
      s = Screen();
      continue;
    }
    if (e.up) {
      item = (item == 0) ? (uint8_t)(ITEM_COUNT - 1) : (uint8_t)(item - 1);
    }
    if (e.down) {
      item = (uint8_t)((item + 1) % ITEM_COUNT);
    }

    if (s.needsPaint()) {
      drawMenu(item);
      s.painted();
    }
    s.pace();
  }

  Buttons::waitForRelease();
  // THE HOLD LATCH, NOT DEBOUNCE REPAIR. This loop reads taps and holds
  // through Buttons' own latches, so nothing here went behind that module's
  // back -- but a button still down on the way out (one waitForRelease() gave
  // up on) would be handed to the main loop as a press already past its
  // threshold. resync() marks it as already fired, and clears any pending
  // release report the same way.
  Buttons::resync();
}

} // namespace RefMenu
