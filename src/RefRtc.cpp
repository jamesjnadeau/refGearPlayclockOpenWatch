#include "RefRtc.h"

#include <Wire.h>

namespace {

const uint8_t ADDR_DS3231 = 0x68;

// DS3231 registers. Time is 0x00..0x06 -- seconds, minutes, hours, DAY (the
// weekday), DATE, month/century, year -- note that the weekday comes BEFORE
// the date, the same trap the RV-3028 laid and the same reason the decoding
// is host-tested: transposing them reads a PLAUSIBLE WRONG DATE rather than
// failing loudly. 0x0E is control, 0x0F status.
const uint8_t DS3231_TIME    = 0x00;
const uint8_t DS3231_CONTROL = 0x0E;
const uint8_t DS3231_STATUS  = 0x0F;

// Status bit 7: the Oscillator Stop Flag. Set whenever the oscillator has
// stopped -- first power-up included -- and it stays set until software
// clears it, so it is how a DS3231 says its time means nothing. This is the
// RV-3028's PORF wearing Maxim's name.
const uint8_t DS3231_OSF = 0x80;

// Status bit 3: EN32kHz, on from the factory. The 32 kHz output drives
// nothing on the OSW board, so it is switched off in begin().
const uint8_t DS3231_EN32KHZ = 0x08;

uint8_t fromBcd(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }
uint8_t toBcd(uint8_t v) { return (uint8_t)(((v / 10) << 4) | (v % 10)); }

bool readRegisters(uint8_t reg, uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(ADDR_DS3231);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  if (Wire.requestFrom(ADDR_DS3231, len) != len) {
    return false;
  }
  for (uint8_t i = 0; i < len; i++) {
    buf[i] = Wire.read();
  }
  return true;
}

bool writeRegisters(uint8_t reg, const uint8_t *buf, uint8_t len) {
  Wire.beginTransmission(ADDR_DS3231);
  Wire.write(reg);
  for (uint8_t i = 0; i < len; i++) {
    Wire.write(buf[i]);
  }
  return Wire.endTransmission() == 0;
}

// Fill in tm_wday and normalise, so callers only have to supply the date.
void normalise(struct tm &t) {
  t.tm_isdst = -1;
  const time_t e = mktime(&t);
  if (e != (time_t)-1) {
    struct tm copy;
    localtime_r(&e, &copy);
    t = copy;
  }
}

} // namespace

bool RefRtc::begin() {
  Wire.beginTransmission(ADDR_DS3231);
  _present = Wire.endTransmission() == 0;
  if (!_present) {
    return false;
  }

  // Control: INTCN set, both alarm interrupt enables clear, square wave off.
  // 0x04 is also the register's power-on value, written anyway because a
  // board that has been through other firmware may not be at delivery
  // defaults -- and an alarm nobody clears would hold the INT line low for
  // good on a pin this project deliberately leaves alone (board.h).
  const uint8_t control = 0x04;
  if (!writeRegisters(DS3231_CONTROL, &control, 1)) {
    return false;
  }

  // Switch the 32 kHz output off; it drives nothing. Read-modify-write so OSF
  // and the rest of the status register are left saying what they were
  // saying -- clearing OSF here, before anyone has set the time, would be
  // exactly the plausible-wrong-time bug timeIsValid() exists to prevent.
  uint8_t status;
  if (!readRegisters(DS3231_STATUS, &status, 1)) {
    return false;
  }
  if (status & DS3231_EN32KHZ) {
    status &= (uint8_t)~DS3231_EN32KHZ;
    return writeRegisters(DS3231_STATUS, &status, 1);
  }
  return true;
}

bool RefRtc::timeIsValid() {
  if (!_present) {
    return false;
  }
  uint8_t status;
  if (!readRegisters(DS3231_STATUS, &status, 1)) {
    return false;
  }
  return (status & DS3231_OSF) == 0;
}

// DS3231 registers 0x00..0x06: seconds, minutes, hours, weekday, date,
// month/century, year. The century bit (month register, bit 7) is masked off
// and ignored: the year is pinned to 2000..2099, which is what the +100 below
// encodes and what RefSetTime's YEAR_MIN/YEAR_MAX offer.
bool RefRtc::read(struct tm &out) {
  if (!_present || !timeIsValid()) {
    return false;
  }

  uint8_t r[7];
  if (!readRegisters(DS3231_TIME, r, 7)) {
    return false;
  }
  out.tm_sec = fromBcd(r[0] & 0x7F);
  out.tm_min = fromBcd(r[1] & 0x7F);
  // 24-hour mode: bit 6 (12/24) is written as 0 by set(). In 12-hour mode
  // bit 5 would be AM/PM and this mask would fold it into the tens digit --
  // so if that mode is ever wanted, this line changes too.
  out.tm_hour = fromBcd(r[2] & 0x3F);
  out.tm_mday = fromBcd(r[4] & 0x3F);
  out.tm_mon = fromBcd(r[5] & 0x1F) - 1;
  out.tm_year = fromBcd(r[6]) + 100;

  // A chip that has never been set reports nonsense; catch anything out of
  // range rather than normalising it into a plausible wrong date.
  if (out.tm_mon < 0 || out.tm_mon > 11 || out.tm_mday < 1 ||
      out.tm_mday > 31 || out.tm_hour > 23 || out.tm_min > 59) {
    return false;
  }
  normalise(out);
  return true;
}

bool RefRtc::set(const struct tm &in) {
  if (!_present) {
    return false;
  }
  struct tm t = in;
  normalise(t);

  const uint8_t r[7] = {
      toBcd((uint8_t)t.tm_sec),
      toBcd((uint8_t)t.tm_min),
      toBcd((uint8_t)t.tm_hour), // bit 6 low: 24 hour mode
      // DS3231 weekday runs 1..7 and is only ever compared to itself; tm_wday
      // runs 0..6, so Sunday is stored as 1.
      (uint8_t)(t.tm_wday + 1),
      toBcd((uint8_t)t.tm_mday),
      toBcd((uint8_t)(t.tm_mon + 1)), // century bit written 0: 2000..2099
      toBcd((uint8_t)(t.tm_year % 100)),
  };
  if (!writeRegisters(DS3231_TIME, r, 7)) {
    return false;
  }

  // The oscillator-stop flag has to be cleared by hand, or every later read
  // would go on reporting the time as unset. Read back rather than writing a
  // bare zero, so the chip's other status bits are left as they were.
  uint8_t status;
  if (!readRegisters(DS3231_STATUS, &status, 1)) {
    return false;
  }
  status &= (uint8_t)~DS3231_OSF;
  return writeRegisters(DS3231_STATUS, &status, 1);
}

time_t RefRtc::epoch() {
  struct tm t;
  if (!read(t)) {
    return 0;
  }
  return mktime(&t);
}
