#include "PlayClock.h"

#include "settings.h"

void PlayClock::reset() {
  _state    = STATE_IDLE;
  _duration = 0;
  _shown    = 0;
  _mark     = {MARK_NONE, 0};
}

void PlayClock::start(const RefSport::Preset &p, uint16_t seconds,
                      uint32_t nowMs) {
  // The timestamp is taken from what the caller passed, which it read BEFORE
  // buzzing the confirmation and painting the screen. The clock starts when
  // the hold registered, not when the watch finished reacting to it.
  _startMs  = nowMs;
  _preset   = p;
  _duration = seconds;
  _shown    = seconds;
  _state    = STATE_RUNNING;
  _mark     = {MARK_NONE, 0};
}

MarkPlan PlayClock::markFor(const RefSport::Preset &p, uint16_t secondsLeft) {
  if (secondsLeft == 0) {
    return {MARK_EXPIRE, 1};
  }
  if (p.finalCountdownFrom > 0 && secondsLeft <= p.finalCountdownFrom) {
    return {MARK_TICK, 1};
  }
  if (WARNING_BUZZ_COUNT > 0 && p.warnAtSeconds > 0 &&
      secondsLeft == p.warnAtSeconds) {
    return {MARK_WARN, WARNING_BUZZ_COUNT};
  }
  if (WARNING_BUZZ_COUNT_2 > 0 && p.warn2AtSeconds > 0 &&
      secondsLeft == p.warn2AtSeconds) {
    return {MARK_WARN, WARNING_BUZZ_COUNT_2};
  }
  return {MARK_NONE, 0};
}

bool PlayClock::tick(uint32_t nowMs) {
  _mark = {MARK_NONE, 0};
  if (_state != STATE_RUNNING) {
    return false;
  }

  // Unsigned difference, so this is correct across the millis() wrap at 49.7
  // days. Comparing the two timestamps directly would not be.
  const uint32_t elapsedMs = nowMs - _startMs;
  const uint32_t totalMs   = (uint32_t)_duration * 1000UL;

  const uint16_t left = elapsedMs >= totalMs
                            ? 0
                            : (uint16_t)(_duration - elapsedMs / 1000UL);
  if (left == _shown) {
    return false;
  }
  _shown = left;
  _mark  = markFor(_preset, left);

  if (left == 0) {
    _state     = STATE_EXPIRED;
    _expiredAt = nowMs + EXPIRED_HOLD_MS;
  }
  return true;
}

bool PlayClock::expiredHoldDone(uint32_t nowMs) const {
  if (_state != STATE_EXPIRED) {
    return false;
  }
  return (int32_t)(nowMs - _expiredAt) >= 0;
}
