#ifndef REF_SET_TIME_H
#define REF_SET_TIME_H

#include <stdint.h>

// Setting the clock by hand.
//
// THIS IS NOW THE ONLY WAY THIS WATCH LEARNS THE TIME. The parent had three
// sources -- the RTC, NTP over Wi-Fi, and BLE Current Time Service -- and two
// of the three are gone with the radio. Nothing replaces them but the user.
//
// It runs about once a year at +/-1 ppm, and again after every full discharge,
// because VBACKUP is tied off and the clock does not survive the cell being
// disconnected. Which means the user will have forgotten how it works EVERY
// SINGLE TIME. So it has to be obvious, and it has to be impossible to leave
// in a half-set state -- there is no "cancel" here, only "advance until it
// commits".
//
// Pure logic, no Arduino and no I2C: the caller reads the fields out and
// writes them to the RTC. That is what lets the whole state machine be tested
// on a host, which matters for a screen almost nobody will ever see twice.
class RefSetTime {
public:
  enum Field : uint8_t {
    FIELD_HOUR = 0,
    FIELD_MINUTE,
    FIELD_YEAR,
    FIELD_MONTH,
    FIELD_DAY,
    FIELD_COUNT,
  };

  // Seed from whatever the clock currently says -- or, when it says nothing,
  // from a sensible starting point the caller picks. Starts on the hour.
  void begin(int year, uint8_t month, uint8_t day, uint8_t hour,
             uint8_t minute);

  Field field() const { return _field; }
  bool committed() const { return _committed; }

  int year() const { return _year; }
  uint8_t month() const { return _month; }
  uint8_t day() const { return _day; }
  uint8_t hour() const { return _hour; }
  uint8_t minute() const { return _minute; }

  // Move the current field. Every field WRAPS rather than clamping: a user
  // who overshoots 59 minutes should not have to press down 58 times, and a
  // field that sticks at its limit reads as a broken button.
  void up();
  void down();

  // Move to the next field, committing after the last one.
  void advance();

  // Move back to the previous field. INERT ON THE FIRST ONE, and inert once
  // committed: this is a cursor move, not an undo. A BACK that could also
  // abandon the screen would abandon it by accident, and an abandoned
  // set-time screen is the one outcome this class exists to prevent.
  //
  // Added in Task 25 Step 3 for the menu, which needs the parent's
  // MENU-forward / BACK-back interaction. Task 24 built the forward walk only,
  // because nothing had yet asked to go back.
  void back();

  // Set a field directly. Used by the tests and by any caller that already
  // knows the value; setMonth() and setDay() re-clamp the day.
  void setYear(int y);
  void setMonth(uint8_t m);
  void setDay(uint8_t d);
  void setHour(uint8_t h);
  void setMinute(uint8_t m);

  // Move the wall clock by whole hours, rolling the date if it crosses
  // midnight. This is the daylight-saving control: it is a SHIFT rather than
  // a re-entry into the set screen, because twice a year the only thing that
  // needs to change is the hour and asking for the date again invites an
  // error in a field that was already right.
  void shiftHour(int delta);

  // How many days the current month has, leap years included.
  static uint8_t daysInMonth(int year, uint8_t month);
  static bool isLeapYear(int year);

  // The range the year field walks. The DS3231's year register runs 00..99
  // and this firmware pins its century to 2000..2099 (RefRtc ignores the
  // century bit), so a year outside that cannot be stored and must not be
  // offered.
  static const int YEAR_MIN = 2024;
  static const int YEAR_MAX = 2099;

private:
  Field _field = FIELD_HOUR;
  bool _committed = false;
  int _year = YEAR_MIN;
  uint8_t _month = 1;
  uint8_t _day = 1;
  uint8_t _hour = 0;
  uint8_t _minute = 0;

  void step(int delta);
  void clampDay();
};

#endif // REF_SET_TIME_H
