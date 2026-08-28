# refGearPlayclockOpenWatch

An implementation of the refGear Playclock firmware for the
[Open-Smartwatch](https://open-smartwatch.github.io/) platform — a purpose
built play clock for officiating, ported from
[refGearPlayclockWatch](https://github.com/jamesjnadeau/refGearPlayclockWatch)
onto the Open-Smartwatch **Light edition V3.3** hardware (ESP32-PICO-D4,
GC9A01 240×240 round LCD, DS3231MZ RTC, three side buttons).

This is a **standalone firmware**: it drives the OSW hardware directly and
does not depend on the open-smartwatch-os codebase or on the parent repo.
The display is black with white text throughout, as this port requires; the
one exception is the expired state, which inverts the whole panel so a `00`
reads across a field.

## Controls

```
   [RESET]  ─────  ┌────────┐  ─────  [UP]     hold ½s → long clock (40s)
   hardware reset  │  240   │
                   │  × 240 │
   [SELECT] ─────  └────────┘  ─────  [DOWN]   hold ½s → short clock (25s)
   hold ½s, release → menu (on ready screen) / clear the clock
   hold ~5s         → sleep;  hold ~5s again to wake
```

- **A short tap never does anything.** A button brushed mid-game cannot
  reset the play clock. Only holds act, exactly as in the parent firmware.
- **Top right (UP)** — hold to start or restart the long clock.
- **Bottom right (DOWN)** — hold to start or restart the short clock.
- **Bottom left (SELECT)** — hold half a second and release: opens the
  settings menu from the ready screen, or clears a running/expired clock back
  to it. Keep holding to **five seconds** and the watch goes to sleep
  (display dark, light sleep); hold the same button five seconds to wake.
- **Top left** is the OSW's hardware **RESET**. It is wired to the ESP32's EN
  line, not to a GPIO, so firmware cannot read or remap it — the parent's
  top-left sleep button has no pin to land on, which is why sleep moved onto
  the SELECT hold. A reset is harmless: the sport and custom preset live in
  NVS and the time lives in the DS3231, so the watch boots straight back to
  the ready screen with everything but a running countdown intact.

In the menu: **UP/DOWN** move, **tap SELECT** chooses / advances a field,
**hold SELECT** goes back / exits. The menu times out to the ready screen
after 15 s of inactivity, and a half-finished time or preset edit is never
committed.

## What it keeps from the parent

- The four-state play clock (idle / running / expired), recomputed from the
  start timestamp so it can never run slow, with per-sport warning marks and
  a final-countdown tick. Sport presets: Football, Lacrosse, Base NCAA,
  Base NFHS, Soft NCAA, Soft NFHS, plus an editable Custom slot — all
  persisted (NVS here, where the parent had emulated flash).
- **The same sleep shape.** The parent slept in STM32 Stop2, which retains
  SRAM; this port uses the ESP32's **light sleep**, which also retains SRAM,
  so there is no reconstruct-on-boot machinery — a conventional `loop()`
  that sleeps between events, any button wakes the core, and only the ~5 s
  SELECT hold wakes the watch. (Deep sleep was rejected: it reboots the
  chip, and GPIO 10 — the DOWN button — cannot wake from it at all.)
- The set-time screen (the DS3231's OSF flag means an unset clock is
  **refused**, never shown as a plausible wrong time), the About screen with
  its drift reminder (re-tuned for the DS3231MZ's ±5 ppm), and the
  battery guard that will never sleep the watch on an ADC that has not first
  proven it can read a healthy cell.
- The host-test discipline: every logic-heavy module compiles and runs on a
  laptop against a stub Arduino/Wire/Preferences layer — 162 tests, no
  hardware needed.

## Layout

```
src/     the firmware (PlatformIO src dir)
test/    host tests and the stub layer -- ./test/run.sh
```

## Building

```bash
./check.sh            # host tests, then the target build if pio is installed
./test/run.sh         # just the host tests (needs only g++)
pio run -e playclock  # just the firmware
```

Flashing uses the OSW's usual path: hold the bottom-left button (GPIO 0, the
BOOT strap) and tap RESET to enter the serial bootloader, then
`pio run -e playclock -t upload` through the edge-connector UART.

## Hardware notes

- Pins, active levels and their reasoning are in `src/board.h`, transcribed
  from open-smartwatch-os's `LIGHT_EDITION_V3_3.h`. The three buttons have
  **mixed active levels** (SELECT presses LOW, UP/DOWN press HIGH).
- The battery reading is a **one-point calibration** (`BATT_SCALE` in
  `board.h`, measured against a full cell on real hardware; the shipped OSW
  firmware doesn't trust this divider into volts at all). The sense node is
  the post-mux rail: on battery it reads the cell; on USB it reads USB
  (~4.6 V) and the gauge just clamps at full. The recalibration recipe is in
  `board.h`, and the battery guard's arming rule keeps a wrong scale safe:
  a reading that never looks healthy can warn, but can never sleep the watch.
- The Light edition has **no vibration motor**, so every buzz is a no-op
  (reported once at boot). An OSW edition with a motor gets the parent's
  tick/warn/expire patterns back by building with `-D REF_VIB_GPIO=<pin>`
  (see `board.h`). The `playclock_vib` environment exists purely so
  `./check.sh` compiles and warning-checks that motor path on every run —
  don't flash it onto a Light.

**Nothing here has been run on a physical watch yet.** The host tests pass
and the target build compiles clean; button feel, panel timing, sleep
current and the battery scale are all bring-up questions.
