#!/usr/bin/env bash
# Host tests. No board, no embedded toolchain, no PlatformIO.
#
#   ./test/run.sh
#
# The point of the stub layer is that everything logic-heavy in this firmware
# -- the pin map, the seven-segment placement, the panel layout, the debounce,
# the menu's row list, the sport table, the RTC's BCD and the store's
# fallbacks -- can be compiled and tested on a laptop. It is the discipline
# this project's parent (refGearPlayclockWatch) proved out, ported with it.
set -euo pipefail
cd "$(dirname "$0")/.."

CXX="${CXX:-g++}"
OUT=".test-build"
mkdir -p "$OUT"

# Suite -> extra sources it needs compiling alongside it.
declare -A SOURCES=(
  [board_test]=""
  [segments_test]="src/RefSegments.cpp"
  [layout_test]=""
  [buttons_test]="src/Buttons.cpp"
  [settime_test]="src/RefSetTime.cpp"
  [rtc_test]="src/RefRtc.cpp"
  [drift_test]="src/RefDrift.cpp"
  [store_test]="src/RefStore.cpp"
  [sport_test]="src/RefSport.cpp src/RefStore.cpp"
  [menu_items_test]="src/RefMenuItems.cpp src/RefSport.cpp src/RefStore.cpp"
  [custom_edit_test]="src/RefCustomEdit.cpp"
  [playclock_test]="src/PlayClock.cpp"
  [battguard_test]="src/BattGuard.cpp"
)

RAN=""
for t in board_test segments_test layout_test buttons_test \
         settime_test rtc_test drift_test store_test sport_test \
         menu_items_test custom_edit_test playclock_test battguard_test; do
  # test/stub comes FIRST on the include path so <Arduino.h>, <Wire.h> and
  # <Preferences.h> resolve to the stubs rather than to anything a
  # system-wide Arduino install might provide.
  $CXX -std=c++17 -Wall -Wextra -Werror -I test/stub -I test -I src \
       -o "$OUT/$t" "test/$t.cpp" ${SOURCES[$t]}
  echo "=== $t"
  "$OUT/$t"
  RAN="$RAN $t"
  echo
done

# EVERY SUITE IN THE TABLE HAS TO APPEAR IN THE LOOP. A table entry is a claim
# that the suite runs; this checks the claim, so a suite cannot spend a
# release declared, compiled by nobody and run by nobody -- passing in the
# dark.
echo "=== suite_table"
for t in "${!SOURCES[@]}"; do
  case " $RAN " in
    *" $t "*) ;;
    *) echo "FAIL: $t is in SOURCES but is never run" >&2; exit 1 ;;
  esac
done
echo "  all ${#SOURCES[@]} suites in the table ran"
