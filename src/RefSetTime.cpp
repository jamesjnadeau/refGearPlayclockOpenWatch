#include "RefSetTime.h"

namespace {

// Wrap `v` into [lo, hi] after a step of +/-1. Written once because every
// field wants it and each open-coded copy is a chance to get an endpoint
// wrong -- the classic being a minute field that wraps 59 to 1.
int wrapped(int v, int lo, int hi) {
  const int span = hi - lo + 1;
  return lo + ((v - lo) % span + span) % span;
}

} // namespace

bool RefSetTime::isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint8_t RefSetTime::daysInMonth(int year, uint8_t month) {
  static const uint8_t days[12] = {31, 28, 31, 30, 31, 30,
                                   31, 31, 30, 31, 30, 31};
  if (month < 1 || month > 12) {
    return 31;
  }
  if (month == 2 && isLeapYear(year)) {
    return 29;
  }
  return days[month - 1];
}

void RefSetTime::begin(int year, uint8_t month, uint8_t day, uint8_t hour,
                       uint8_t minute) {
  _field = FIELD_HOUR;
  _committed = false;
  _year = year < YEAR_MIN ? YEAR_MIN : (year > YEAR_MAX ? YEAR_MAX : year);
  _month = (month < 1 || month > 12) ? 1 : month;
  _day = (day < 1) ? 1 : day;
  _hour = hour > 23 ? 0 : hour;
  _minute = minute > 59 ? 0 : minute;
  clampDay();
}

void RefSetTime::clampDay() {
  const uint8_t last = daysInMonth(_year, _month);
  if (_day > last) {
    _day = last;
  }
  if (_day < 1) {
    _day = 1;
  }
}

void RefSetTime::setYear(int y) {
  _year = wrapped(y, YEAR_MIN, YEAR_MAX);
  // A year change can move February. 29 February 2024 becoming 2025 has to
  // become the 28th, or the RTC is handed a date that does not exist.
  clampDay();
}

void RefSetTime::setMonth(uint8_t m) {
  _month = (uint8_t)wrapped(m, 1, 12);
  clampDay();
}

void RefSetTime::setDay(uint8_t d) {
  _day = (uint8_t)wrapped(d, 1, daysInMonth(_year, _month));
}

void RefSetTime::setHour(uint8_t h) { _hour = (uint8_t)wrapped(h, 0, 23); }
void RefSetTime::setMinute(uint8_t m) { _minute = (uint8_t)wrapped(m, 0, 59); }

void RefSetTime::step(int delta) {
  switch (_field) {
  case FIELD_HOUR:
    _hour = (uint8_t)wrapped((int)_hour + delta, 0, 23);
    break;
  case FIELD_MINUTE:
    _minute = (uint8_t)wrapped((int)_minute + delta, 0, 59);
    break;
  case FIELD_YEAR:
    setYear(_year + delta);
    break;
  case FIELD_MONTH:
    setMonth((uint8_t)wrapped((int)_month + delta, 1, 12));
    break;
  case FIELD_DAY:
    _day = (uint8_t)wrapped((int)_day + delta, 1, daysInMonth(_year, _month));
    break;
  default:
    break;
  }
}

void RefSetTime::up() { step(+1); }
void RefSetTime::down() { step(-1); }

void RefSetTime::advance() {
  if (_committed) {
    return;
  }
  if (_field + 1 >= FIELD_COUNT) {
    // THE LAST ADVANCE COMMITS. There is no separate confirm step and no
    // cancel: a set-time screen that can be left half-finished leaves the
    // watch showing a plausible wrong time, which is the one outcome this
    // whole task exists to prevent.
    _committed = true;
    return;
  }
  _field = (Field)(_field + 1);
}

void RefSetTime::back() {
  if (_committed || _field == FIELD_HOUR) {
    return;
  }
  _field = (Field)(_field - 1);
}

void RefSetTime::shiftHour(int delta) {
  int h = (int)_hour + delta;
  while (h < 0) {
    h += 24;
    // Walk the date back a day, month and year included.
    if (_day > 1) {
      _day--;
    } else {
      _month = (uint8_t)(_month == 1 ? 12 : _month - 1);
      if (_month == 12) {
        _year = _year > YEAR_MIN ? _year - 1 : YEAR_MIN;
      }
      _day = daysInMonth(_year, _month);
    }
  }
  while (h > 23) {
    h -= 24;
    if (_day < daysInMonth(_year, _month)) {
      _day++;
    } else {
      _day = 1;
      _month = (uint8_t)(_month == 12 ? 1 : _month + 1);
      if (_month == 1) {
        _year = _year < YEAR_MAX ? _year + 1 : YEAR_MAX;
      }
    }
  }
  _hour = (uint8_t)h;
}
