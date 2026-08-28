#ifndef REF_CUSTOM_EDIT_H
#define REF_CUSTOM_EDIT_H

#include <stdint.h>

#include "RefSport.h"

// Editing the Custom preset's five numbers.
//
// SPLIT OUT OF THE MENU FOR THE REASON RefSetTime WAS: the parent's
// editCustom() interleaved the field walk, the wrap rules and the panel
// drawing in one 90-line loop, so none of the rules could be checked without
// a watch. They are not obvious rules.
//
//   THE TWO CLOCKS BOTTOM OUT AT 1 AND THE THREE MARKS DO NOT. A clock of 0
//   would expire the instant it started, so the long and short fields wrap
//   199 -> 1. A mark of 0 means "off", which is a value somebody actively
//   wants, so those wrap 199 -> 0. Getting that backwards gives a watch whose
//   second warning cannot be turned off.
//
//   THERE IS NO CANCEL, AND A TIMEOUT IS NOT ONE. Like RefSetTime, this
//   commits only by advancing past the last field. The menu's loop drops out
//   on MENU_TIMEOUT_MS as well, and committed() stays false on that path, so
//   a half-finished edit left on the bench is never persisted.
//
// The caller owns the persistence: read the five values out on commit and
// hand them to RefSport::setCustom(), which clamps them again on the way in.
//
// Pure logic, no Arduino and no I2C.
class RefCustomEdit {
public:
  enum Field : uint8_t {
    FIELD_LONG = 0,
    FIELD_SHORT,
    FIELD_WARN,
    FIELD_WARN2,
    FIELD_FINAL,
    FIELD_COUNT,
  };

  // Seed from the preset as stored. Starts on the long clock.
  void begin(const RefSport::Preset &p);

  Field field() const { return _field; }
  bool committed() const { return _committed; }

  uint16_t value(Field f) const;

  // The row's name, for the screen. Never null.
  static const char *label(Field f);

  // The lowest value this field may take: 1 for the two clocks, 0 for the
  // three marks. Exposed because it is the rule most worth asserting.
  static uint16_t floorOf(Field f);

  // Move the current field, wrapping at both ends.
  void up();
  void down();

  // Next field, committing past the last one. Does nothing once committed.
  void advance();

  // Previous field. Inert on the first one, which is what makes BACK safe to
  // lean on -- it can never leave the screen half-set.
  void back();

private:
  Field    _field     = FIELD_LONG;
  bool     _committed = false;
  uint16_t _value[FIELD_COUNT] = {0, 0, 0, 0, 0};

  void step(int delta);
};

#endif // REF_CUSTOM_EDIT_H
