#include "RefCustomEdit.h"

void RefCustomEdit::begin(const RefSport::Preset &p) {
  _field     = FIELD_LONG;
  _committed = false;
  _value[FIELD_LONG]  = p.longSeconds;
  _value[FIELD_SHORT] = p.shortSeconds;
  _value[FIELD_WARN]  = p.warnAtSeconds;
  _value[FIELD_WARN2] = p.warn2AtSeconds;
  _value[FIELD_FINAL] = p.finalCountdownFrom;

  // A stored preset should already be inside these bounds -- RefSport clamps
  // on the way in -- but seeding from one that is not would put the cursor on
  // a value the wrap arithmetic below cannot reach again.
  for (uint8_t f = 0; f < FIELD_COUNT; f++) {
    const uint16_t lo = floorOf((Field)f);
    if (_value[f] < lo) {
      _value[f] = lo;
    } else if (_value[f] > RefSport::MAX_SECONDS) {
      _value[f] = RefSport::MAX_SECONDS;
    }
  }
}

uint16_t RefCustomEdit::floorOf(Field f) {
  return (f == FIELD_LONG || f == FIELD_SHORT) ? RefSport::MIN_CLOCK_SECONDS
                                               : 0;
}

const char *RefCustomEdit::label(Field f) {
  switch (f) {
  case FIELD_LONG:  return "Long";
  case FIELD_SHORT: return "Short";
  case FIELD_WARN:  return "Warn 1";
  case FIELD_WARN2: return "Warn 2";
  case FIELD_FINAL: return "Final";
  default:          return "";
  }
}

uint16_t RefCustomEdit::value(Field f) const {
  return f < FIELD_COUNT ? _value[f] : 0;
}

void RefCustomEdit::step(int delta) {
  if (_committed) {
    return;
  }
  const int lo = (int)floorOf(_field);
  const int hi = (int)RefSport::MAX_SECONDS;
  int v = (int)_value[_field] + delta;
  if (v > hi) {
    v = lo;
  } else if (v < lo) {
    v = hi;
  }
  _value[_field] = (uint16_t)v;
}

void RefCustomEdit::up() { step(+1); }
void RefCustomEdit::down() { step(-1); }

void RefCustomEdit::advance() {
  if (_committed) {
    return;
  }
  if (_field + 1 >= FIELD_COUNT) {
    _committed = true;
    return;
  }
  _field = (Field)(_field + 1);
}

void RefCustomEdit::back() {
  if (_committed || _field == FIELD_LONG) {
    return;
  }
  _field = (Field)(_field - 1);
}
