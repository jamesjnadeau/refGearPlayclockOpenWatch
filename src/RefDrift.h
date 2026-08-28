#ifndef REF_DRIFT_H
#define REF_DRIFT_H

#include <stdint.h>
#include <time.h>

// How long ago the clock was set, and when to say something about it.
//
// PORTED WITH NEW NUMBERS. The parent's RV-3028 was a +/-1 ppm part; the
// Open-Smartwatch Light's DS3231MZ is specified +/-5 ppm, so the same
// invisible slow failure runs five times faster here -- about 2 minutes 40
// seconds a year. The module's job is unchanged: a watch that silently
// drifts is a watch you stop trusting, and turning that into a visible line
// on the About screen costs one timestamp (RefStore's KEY_CLOCK_SET_AT --
// this platform has NVS, so the parent's open question about where the
// timestamp lives is simply answered).
//
// PURE ARITHMETIC, no Arduino and no I2C, like RefSegments.
namespace RefDrift {

// Maxim's specification for the fitted DS3231MZ, and the whole reason this
// module exists.
static const int32_t RTC_PPM = 5;

// How long before the About screen starts mentioning it. Three months at
// 5 ppm is about 39 seconds of worst-case drift -- the same "worth checking
// before setting a play clock by it" threshold the parent drew at a year of
// its 1 ppm part.
static const uint32_t REMIND_AFTER_MONTHS = 3;

// Whole months between the two instants, floored. Calendar months, not
// 30-day blocks: "set 14 months ago" is what a person understands, and
// 14 * 30 days is not 14 months.
uint32_t monthsSince(time_t setAt, time_t now);

// Seconds of drift the part is specified to have accumulated. Signed, because
// a crystal can run fast or slow and +/-1 ppm is a bound in both directions --
// this is the WORST CASE magnitude, not a prediction of the error.
uint32_t worstCaseDriftSeconds(time_t setAt, time_t now);

// True when the About screen should say something.
bool shouldRemind(time_t setAt, time_t now);

// "set 14 months ago", or "clock never set". Writes at most `len` bytes
// including the terminator.
void describe(char *out, uint32_t len, time_t setAt, time_t now);

} // namespace RefDrift

#endif // REF_DRIFT_H
