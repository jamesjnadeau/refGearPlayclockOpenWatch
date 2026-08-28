// Where everything lands on the 240 x 240 round panel.
//
// SEPARATED FROM RefDisplay SO IT CAN BE CHECKED ON A HOST, for the same
// reason RefSegments is: this is arithmetic, and arithmetic that only runs on
// a device is arithmetic nobody has checked. Nothing here includes Arduino or
// the panel.
//
// RE-DERIVED FROM THE PARENT'S 128 x 128, NOT SCALED BLINDLY -- and this port
// adds a constraint the parent never had: THE PANEL IS ROUND. A GC9A01 shows
// a 240 px circle out of its square framebuffer, so "fits on the screen" is no
// longer a pair of one-dimensional comparisons. rectVisible() below does the
// two-dimensional check, and the static_asserts use it for every element that
// sits away from the centre. An element that leaks outside the circle does
// not error at run time -- the pixels are simply behind the bezel -- which is
// exactly the silent failure the parent's asserts existed to catch.

#ifndef REF_LAYOUT_H
#define REF_LAYOUT_H

#include <stdint.h>

#include "RefSegments.h"

// The panel. 240 x 240 addressed, of which a centred circle of radius 120 is
// physically visible. One bit per pixel in RefPanel's canvas: 7.2 kB.
static constexpr int16_t SCREEN_W = 240;
static constexpr int16_t SCREEN_H = 240;
static constexpr int16_t CIRCLE_R = 120;

// True when the point is inside the visible circle. Everything is measured
// from the centre at (120, 120); <= keeps the boundary pixel itself legal.
static constexpr bool pointVisible(int32_t x, int32_t y) {
  return (x - CIRCLE_R) * (x - CIRCLE_R) + (y - CIRCLE_R) * (y - CIRCLE_R) <=
         (int32_t)CIRCLE_R * CIRCLE_R;
}

// True when the whole axis-aligned rectangle is inside the circle -- which is
// true exactly when its four corners are.
static constexpr bool rectVisible(int32_t x, int32_t y, int32_t w, int32_t h) {
  return pointVisible(x, y) && pointVisible(x + w, y) &&
         pointVisible(x, y + h) && pointVisible(x + w, y + h);
}

// --- Header strip -----------------------------------------------------------
// Wall clock left, battery gauge right, one rule underneath. 20% of the panel
// rather than the parent's 18%, because the top of a circle is the narrow
// part: dropping the strip a few pixels buys the clock text a wider chord.
static constexpr int16_t HEADER_RULE_Y   = 48;
static constexpr int16_t HEADER_BASELINE = 38;
static constexpr int16_t CLOCK_X         = 56;

// FreeMonoBold12pt7b is monospaced with a 14 px advance and about 17 px of
// cap above the baseline. The clock is at most five characters ("23:59").
static constexpr int16_t HEADER_FONT_ADVANCE = 14;
static constexpr int16_t HEADER_FONT_ASCENT  = 17;
static constexpr int16_t CLOCK_MAX_CHARS     = 5;
static constexpr int16_t CLOCK_W = CLOCK_MAX_CHARS * HEADER_FONT_ADVANCE;

// The gauge. The nub on the right is the battery's positive terminal and
// reads as a battery at any size.
static constexpr int16_t BATT_W   = 28;
static constexpr int16_t BATT_H   = 14;
static constexpr int16_t BATT_NUB = 3;
static constexpr int16_t BATT_X   = 150;
static constexpr int16_t BATT_Y   = 24;

// --- Body -------------------------------------------------------------------
// From the header rule to the bottom edge. ALL of it: there is no footer, no
// reserved strip and no margin held back for text that is no longer drawn.
static constexpr int16_t BODY_Y0 = HEADER_RULE_Y + 1;
static constexpr int16_t BODY_Y1 = SCREEN_H - 1;
static constexpr int16_t BODY_H  = BODY_Y1 - BODY_Y0 + 1;

// The running countdown. The parent's 46 x 84 digit filled a 128-wide body;
// 64 x 120 is the same idea at this size, checked against the circle below.
static constexpr SegStyle STYLE_BIG   = {64, 120, 14, 12};

// The idle screen stacks two values, so each gets roughly half the body.
static constexpr SegStyle STYLE_SMALL = {36, 60, 8, 8};

// The menu's set-time field, between the two.
static constexpr SegStyle STYLE_MED   = {40, 64, 9, 8};

static constexpr int16_t BIG_Y  = BODY_Y0 + (BODY_H - STYLE_BIG.h) / 2;
static constexpr int16_t ROW1_Y = BODY_Y0 + 8;
static constexpr int16_t ROW2_Y = ROW1_Y + STYLE_SMALL.h + 20;

// The marker pointing at the button that starts each idle row. Its tip sits
// at MARKER_TIP_X on the row's centreline, pointing right, at the two
// right-hand buttons.
static constexpr int16_t MARKER_W     = 14;
static constexpr int16_t MARKER_H     = 16;
static constexpr int16_t MARKER_TIP_X = 224;

// --- What has to be true ----------------------------------------------------
// Written as asserts rather than as comments, because a layout that overflows
// does not error at run time -- Adafruit_GFX clips silently at the square
// edge and the bezel clips silently at the circle -- and a digit half hidden
// looks like a font problem rather than a layout one.

static_assert(HEADER_RULE_Y < SCREEN_H / 4,
              "the header is eating the body");
static_assert(HEADER_BASELINE < HEADER_RULE_Y,
              "the clock's baseline is below its own rule");
static_assert(CLOCK_X + CLOCK_W < BATT_X,
              "the clock and the battery gauge overlap");
static_assert(rectVisible(CLOCK_X, HEADER_BASELINE - HEADER_FONT_ASCENT,
                          CLOCK_W, HEADER_FONT_ASCENT),
              "the wall clock leaks outside the visible circle");
static_assert(rectVisible(BATT_X, BATT_Y, BATT_W + BATT_NUB, BATT_H),
              "the battery gauge leaks outside the visible circle");
static_assert(BATT_Y + BATT_H < HEADER_RULE_Y,
              "the battery gauge crosses the header rule");

// The widest the countdown can get is 199: a hundreds bar one thickness wide,
// then a gap, then the two-digit pair. RefSegments computes it; this is the
// same arithmetic, asserted against the circle.
static constexpr int16_t BIG_MAX_W =
    STYLE_BIG.t + STYLE_BIG.gap + 2 * STYLE_BIG.w + STYLE_BIG.gap;
static_assert(rectVisible((SCREEN_W - BIG_MAX_W) / 2, BIG_Y, BIG_MAX_W,
                          STYLE_BIG.h),
              "199 in STYLE_BIG leaks outside the visible circle");
static_assert(BIG_Y >= BODY_Y0,
              "the big countdown starts inside the header");

// An idle row is at most a two-digit pair, centred; its marker rides at the
// right edge on the row's centreline.
static constexpr int16_t SMALL_PAIR_W = 2 * STYLE_SMALL.w + STYLE_SMALL.gap;
static_assert(rectVisible((SCREEN_W - SMALL_PAIR_W) / 2, ROW1_Y, SMALL_PAIR_W,
                          STYLE_SMALL.h),
              "the first idle row leaks outside the visible circle");
static_assert(rectVisible((SCREEN_W - SMALL_PAIR_W) / 2, ROW2_Y, SMALL_PAIR_W,
                          STYLE_SMALL.h),
              "the second idle row leaks outside the visible circle");
static_assert(ROW1_Y + STYLE_SMALL.h < ROW2_Y,
              "the two idle rows overlap");
static_assert(rectVisible(MARKER_TIP_X - MARKER_W,
                          ROW1_Y + STYLE_SMALL.h / 2 - MARKER_H / 2, MARKER_W,
                          MARKER_H),
              "the first row's marker leaks outside the visible circle");
static_assert(rectVisible(MARKER_TIP_X - MARKER_W,
                          ROW2_Y + STYLE_SMALL.h / 2 - MARKER_H / 2, MARKER_W,
                          MARKER_H),
              "the second row's marker leaks outside the visible circle");

// Seven-segment geometry only works while the thickness fits three times over
// with room for the gaps between the bars.
static_assert(STYLE_BIG.h > 4 * STYLE_BIG.t,
              "STYLE_BIG's segments are thicker than its digit is tall");
static_assert(STYLE_SMALL.h > 4 * STYLE_SMALL.t,
              "STYLE_SMALL's segments are thicker than its digit is tall");
static_assert(STYLE_MED.h > 4 * STYLE_MED.t,
              "STYLE_MED's segments are thicker than its digit is tall");

#endif // REF_LAYOUT_H
