#ifndef REF_RTC_H
#define REF_RTC_H

#include <time.h>

#include <stdint.h>

// The watch's real time clock, and the only timebase it has.
//
// THE CHIP CHANGED AND THE CONTRACT DID NOT. The parent drove an RV-3028-C7;
// the Open-Smartwatch Light fits a DS3231MZ at 0x68, so the register map and
// the BCD layout below it are Maxim's. Everything the rest of the firmware
// relies on is carried over intact:
//
//   timeIsValid() IS THE PORF CHECK, WEARING MAXIM'S NAME FOR IT. The
//   DS3231's OSF bit (status register 0x0F, bit 7) latches whenever the
//   oscillator has stopped -- a fresh board, a flat backup supply. THE UI
//   MUST NEVER SHOW A PLAUSIBLE WRONG TIME, so this is the question every
//   screen asks before drawing a clock, and set() is what clears it.
//
//   TALKED TO DIRECTLY OVER Wire IN BCD. A handful of register reads; no RTC
//   library. That is what lets the decoding be tested against the Wire stub
//   on a host -- it is the code most likely to be wrong and least likely to
//   be caught by eye.
//
// One inheritance from the OSW hardware is friendlier than the parent's: the
// DS3231 hangs off the watch's battery through the power mux, so the time
// survives light sleep trivially and survives a full discharge for as long as
// the cell's protection leaves anything at all. An unset clock is still a
// ROUTINE PATH -- every first boot lands there -- just a rarer one.
class RefRtc {
public:
  // Configures the part. Wire must already be started. Returns false if
  // nothing answered at the DS3231's address.
  bool begin();

  bool present() const { return _present; }

  // False when the oscillator has stopped since the time was last set -- the
  // OSF bit.
  bool timeIsValid();

  // Local wall clock. False if the clock could not be read, or if it holds an
  // obviously invalid time.
  bool read(struct tm &out);

  // Set the clock, and clear OSF. `t` is local time; tm_wday is computed here.
  bool set(const struct tm &t);

  // Seconds since the epoch, or 0 if the clock is not set.
  time_t epoch();

private:
  bool _present = false;
};

#endif // REF_RTC_H
