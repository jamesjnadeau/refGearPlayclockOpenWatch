#!/usr/bin/env bash
# Everything the firmware can be checked for without a watch.
#
#   ./check.sh
#
# Two stages: the host tests, and a target build with warnings on. The second
# needs PlatformIO and network access to its registry; the first needs nothing
# but g++, which is the whole point of the stub layer.
set -euo pipefail
cd "$(dirname "$0")"

echo "=== host tests"
./test/run.sh | tail -3

echo
echo "=== target build"
if ! command -v pio >/dev/null 2>&1; then
  echo "SKIP: PlatformIO is not installed. The host tests above are the"
  echo "      gate this environment can run; install PlatformIO (pipx"
  echo "      install platformio) to build for the watch."
  exit 0
fi

pio run -e playclock 2>&1 | tee /tmp/pio-build.log \
  | grep -E "^(RAM|Flash):" || true

# *** THE BUILD HAS TO HAVE HAPPENED BEFORE ITS WARNINGS MEAN ANYTHING. ***
# `pio run` can fail before any compiler output (no network to the registry,
# for one), and a log empty of warnings is then indistinguishable from a
# clean build. The RAM:/Flash: summary is the evidence a build ran.
if ! grep -qE "^(RAM|Flash):" /tmp/pio-build.log; then
  echo "FAIL: the target build produced no size summary, so it did not run." >&2
  echo "      Last lines of /tmp/pio-build.log:" >&2
  tail -5 /tmp/pio-build.log >&2
  exit 1
fi

# -Wall -Wextra are on for src/ only (build_src_flags). The libraries are not
# warning-clean, so ours have to be picked out rather than counted.
if grep -qE "^src/.*(warning|error)" /tmp/pio-build.log; then
  echo "FAIL: warnings in our own sources:" >&2
  grep -E "^src/.*(warning|error)" /tmp/pio-build.log >&2
  exit 1
fi
echo "  no warnings in src/"
