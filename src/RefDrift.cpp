#include "RefDrift.h"

#include <stdio.h>

namespace RefDrift {
namespace {

bool broken(time_t setAt, time_t now, struct tm &a, struct tm &b) {
  if (setAt <= 0 || now <= 0 || now < setAt) {
    return false;
  }
  struct tm ta, tb;
  if (localtime_r(&setAt, &ta) == nullptr || localtime_r(&now, &tb) == nullptr) {
    return false;
  }
  a = ta;
  b = tb;
  return true;
}

} // namespace

uint32_t monthsSince(time_t setAt, time_t now) {
  struct tm a, b;
  if (!broken(setAt, now, a, b)) {
    return 0;
  }
  int months = (b.tm_year - a.tm_year) * 12 + (b.tm_mon - a.tm_mon);
  // Floor to WHOLE months: the 5th of March to the 3rd of April is not a
  // month, however much the month numbers differ by one.
  if (b.tm_mday < a.tm_mday) {
    months--;
  }
  return months < 0 ? 0 : (uint32_t)months;
}

uint32_t worstCaseDriftSeconds(time_t setAt, time_t now) {
  if (setAt <= 0 || now <= setAt) {
    return 0;
  }
  const uint64_t elapsed = (uint64_t)(now - setAt);
  // ppm is parts per million of elapsed time. Done in 64 bits because a
  // decade of seconds times a million overflows 32 long before the answer
  // does.
  return (uint32_t)((elapsed * (uint64_t)RTC_PPM) / 1000000ULL);
}

bool shouldRemind(time_t setAt, time_t now) {
  // NEVER SET IS ALWAYS WORTH SAYING. A clock that has never been set is not
  // a clock that was set a long time ago -- it is the state the watch lands
  // in after every full discharge, and the user has to be told.
  if (setAt <= 0) {
    return true;
  }
  return monthsSince(setAt, now) >= REMIND_AFTER_MONTHS;
}

void describe(char *out, uint32_t len, time_t setAt, time_t now) {
  if (out == nullptr || len == 0) {
    return;
  }
  if (setAt <= 0) {
    snprintf(out, len, "clock never set");
    return;
  }
  const uint32_t m = monthsSince(setAt, now);
  if (m == 0) {
    snprintf(out, len, "set this month");
  } else if (m == 1) {
    snprintf(out, len, "set 1 month ago");
  } else {
    snprintf(out, len, "set %lu months ago", (unsigned long)m);
  }
}

} // namespace RefDrift
