#include "RefDisplay.h"

#include <Fonts/FreeMonoBold12pt7b.h>

#include "RefLayout.h"
#include "RefPanel.h"
#include "RefSegments.h"
#include "board.h"
#include "settings.h"

extern const uint16_t THEME_FG =
    DARK_MODE ? RefPanel::PANEL_WHITE : RefPanel::PANEL_BLACK;
extern const uint16_t THEME_BG =
    DARK_MODE ? RefPanel::PANEL_BLACK : RefPanel::PANEL_WHITE;

namespace RefDisplay {
namespace {

auto &display = RefPanel::display;

// NOT REFERENCES: STATE_EXPIRED paints the whole panel inverted, and the way
// to do that is to swap these two for that one paint rather than to write a
// second set of draw calls. So they are values, set at the top of paint().
uint16_t FG = THEME_FG;
uint16_t BG = THEME_BG;

// Which segments are lit for each digit. Bit order a,b,c,d,e,f,g.
const uint8_t SEG_A = 0x01, SEG_B = 0x02, SEG_C = 0x04, SEG_D = 0x08,
              SEG_E = 0x10, SEG_F = 0x20, SEG_G = 0x40;

const uint8_t DIGIT_SEGMENTS[10] = {
    /* 0 */ SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F,
    /* 1 */ SEG_B | SEG_C,
    /* 2 */ SEG_A | SEG_B | SEG_G | SEG_E | SEG_D,
    /* 3 */ SEG_A | SEG_B | SEG_G | SEG_C | SEG_D,
    /* 4 */ SEG_F | SEG_G | SEG_B | SEG_C,
    /* 5 */ SEG_A | SEG_F | SEG_G | SEG_C | SEG_D,
    /* 6 */ SEG_A | SEG_F | SEG_G | SEG_E | SEG_C | SEG_D,
    /* 7 */ SEG_A | SEG_B | SEG_C,
    /* 8 */ SEG_A | SEG_B | SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,
    /* 9 */ SEG_A | SEG_B | SEG_C | SEG_D | SEG_F | SEG_G,
};

void drawDigit(int16_t x, int16_t y, uint8_t value, const SegStyle &s,
               uint16_t colour) {
  const uint8_t on = DIGIT_SEGMENTS[value % 10];
  const SegRows r = layoutRows(s);

  const int16_t t = s.t;
  const int16_t barW = s.w - 2 * t;
  const int16_t right = x + s.w - t;

  if (on & SEG_A) display.fillRect(x + t, y, barW, t, colour);
  if (on & SEG_B) display.fillRect(right, y + r.upY, t, r.upH, colour);
  if (on & SEG_C) display.fillRect(right, y + r.lowY, t, r.lowH, colour);
  if (on & SEG_D) display.fillRect(x + t, y + s.h - t, barW, t, colour);
  if (on & SEG_E) display.fillRect(x, y + r.lowY, t, r.lowH, colour);
  if (on & SEG_F) display.fillRect(x, y + r.upY, t, r.upH, colour);
  if (on & SEG_G) display.fillRect(x + t, y + r.midY, barW, t, colour);
}

void drawPair(int16_t x, int16_t y, uint16_t value, const SegStyle &s,
              uint16_t colour) {
  if (value > 99) {
    value = 99;
  }
  drawDigit(x, y, (uint8_t)(value / 10), s, colour);
  drawDigit(x + s.w + s.gap, y, (uint8_t)(value % 10), s, colour);
}

// The skinny leading "1": segments b and c only, one thickness wide.
void drawOneBar(int16_t x, int16_t y, const SegStyle &s, uint16_t colour) {
  const SegRows r = layoutRows(s);
  display.fillRect(x, y + r.upY, s.t, r.upH, colour);
  display.fillRect(x, y + r.lowY, s.t, r.lowH, colour);
}

// A countdown value, 0..199, centred horizontally. Where each glyph goes is
// RefSegments' decision, so it can be checked on a host.
void drawCount(uint16_t value, int16_t y, const SegStyle &s, uint16_t colour) {
  const CountLayout l = layoutCount(value, s, SCREEN_W);
  if (l.hundreds) {
    drawOneBar(l.oneX, y, s, colour);
  }
  drawDigit(l.tensX, y, l.tens, s, colour);
  drawDigit(l.onesX, y, l.ones, s, colour);
}

void drawBattery() {
  const float v = batteryVolts();
  float pct = (v - BATT_MIN_V) / (BATT_MAX_V - BATT_MIN_V);
  pct = pct < 0.0f ? 0.0f : (pct > 1.0f ? 1.0f : pct);

  display.drawRect(BATT_X, BATT_Y, BATT_W, BATT_H, FG);
  display.fillRect(BATT_X + BATT_W, BATT_Y + 4, BATT_NUB, BATT_H - 8, FG);

  const int16_t fill = (int16_t)((BATT_W - 4) * pct);
  if (fill > 0) {
    display.fillRect(BATT_X + 2, BATT_Y + 2, fill, BATT_H - 4, FG);
  }
}

void drawHeader(const View &v) {
  char clock[8];
  if (!v.clockValid) {
    snprintf(clock, sizeof(clock), "--:--");
  } else if (CLOCK_24_HOUR) {
    snprintf(clock, sizeof(clock), "%02u:%02u", v.hour, v.minute);
  } else {
    uint8_t h = v.hour % 12;
    if (h == 0) {
      h = 12;
    }
    snprintf(clock, sizeof(clock), "%u:%02u", h, v.minute);
  }

  display.setFont(&FreeMonoBold12pt7b);
  display.setTextColor(FG);
  display.setCursor(CLOCK_X, HEADER_BASELINE);
  display.print(clock);

  drawBattery();
  display.drawFastHLine(0, HEADER_RULE_Y, SCREEN_W, FG);
}

// A solid triangle pointing at the button that starts the value on this row.
// Its tip sits at MARKER_TIP_X so it stays inside the visible circle -- see
// RefLayout.h.
void drawRowMarker(int16_t rowY, const SegStyle &s) {
  const int16_t cy = rowY + s.h / 2;
  display.fillTriangle(MARKER_TIP_X, cy,
                       MARKER_TIP_X - MARKER_W, cy - MARKER_H / 2,
                       MARKER_TIP_X - MARKER_W, cy + MARKER_H / 2, FG);
}

void drawBody(const View &v) {
  if (v.state == STATE_IDLE) {
    // Stack both clocks so they line up with the two right-hand buttons.
    drawCount(v.idleLongSec, ROW1_Y, STYLE_SMALL, FG);
    drawRowMarker(ROW1_Y, STYLE_SMALL);
    drawCount(v.idleShortSec, ROW2_Y, STYLE_SMALL, FG);
    drawRowMarker(ROW2_Y, STYLE_SMALL);
    return;
  }
  drawCount(v.secondsLeft, BIG_Y, STYLE_BIG, FG);
}

} // namespace

float batteryVolts() {
  // The ESP32 core's calibrated read, in millivolts at the pin, scaled up to
  // cell volts through BATT_SCALE -- which is a one-point calibration against
  // a full cell, not a resistor ratio; the whole story is in board.h.
  //
  // ONE READ, DELIBERATELY. BATT_SCALE was calibrated against exactly this
  // measurement method, and any "improvement" here (oversampling, a settling
  // read thrown away) changes what the pin reads and silently invalidates
  // the calibration. Change this and board.h's recipe together or not at all.
  const uint32_t mv = analogReadMilliVolts(PIN_BATT_ADC);
  return (mv / 1000.0f) * BATT_SCALE;
}

extern const int16_t DIGIT_PAIR_W = STYLE_MED.w * 2 + STYLE_MED.gap;
extern const int16_t DIGIT_PAIR_H = STYLE_MED.h;

void drawDigitPair(int16_t x, int16_t y, uint8_t value, bool lit) {
  drawPair(x, y, value, STYLE_MED, lit ? THEME_FG : THEME_BG);
}

void begin() {
  RefPanel::begin();
  display.setTextWrap(false);
}

void paint(const View &v) {
  // THE EXPIRED STATE IS THE WHOLE PANEL, NOT A CAPTION. STATE_EXPIRED paints
  // everything inverted: a fully lit 240 px circle is readable from across a
  // field in a way a caption never was. Done by swapping the two colours for
  // this paint, so there is one set of draw calls and no second layout to
  // keep in step.
  const bool inverted = (v.state == STATE_EXPIRED);
  FG = inverted ? THEME_BG : THEME_FG;
  BG = inverted ? THEME_FG : THEME_BG;

  display.fillScreen(BG);
  drawHeader(v);
  drawBody(v);
}

void render(const View &v) {
  paint(v);
  refresh();
}

void refresh() { RefPanel::refresh(); }

void blank() { RefPanel::blank(); }

void unblank() { RefPanel::unblank(); }

} // namespace RefDisplay
