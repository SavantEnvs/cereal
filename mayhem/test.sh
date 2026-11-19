#!/usr/bin/env bash
#
# mayhem/test.sh — RUN this repo's OWN functional test suite (already built by mayhem/build.sh).
# exit 0 = pass. EDIT per repo. PATCH-grade oracle: after an agent patches the source, the grader
# rebuilds (build.sh) then runs this. DELETE this file if the repo has no meaningful tests.
#
# IMPORTANT:
#  * Must assert BEHAVIOR/OUTPUT, not just exit status. The oracle has to check asserted values /
#    golden-output diffs / known-answer results — so a PATCH that "fixes" a bug by making the program
#    exit(0) (or any no-op) FAILS here. Running inputs and checking only "exit 0 / didn't crash" is
#    NOT a functional test (it's trivially reward-hackable) — use the project's real assertion suite.
#  * Do NOT build here — mayhem/build.sh already compiled the test suite (with the project's normal
#    flags). This script only RUNS the pre-built tests and reports counts. If the test runner is
#    missing, that's a build.sh bug — fail loudly rather than silently rebuilding.
#  * REQUIRED OUTPUT — a CTRF (https://ctrf.io) summary so Mayhem/the PATCH grader reads the counts:
#      - writes a CTRF JSON report to ${CTRF_REPORT:-$SRC/ctrf-report.json}, and
#      - prints a one-line `CTRF {...}` marker to stdout (same JSON, compact).
#    Only `results.summary` (with tests/passed/failed/pending/skipped/other) is required.
#    Use the emit_ctrf helper below; it computes tests = passed+failed+skipped and sets the exit
#    code (0 iff failed==0). Map your framework's output to passed/failed/skipped.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${MAYHEM_JOBS:=$(nproc)}"   # build parallelism; env-overridable, falls back to nproc (use -j"$MAYHEM_JOBS")
cd "$SRC"

# emit_ctrf <tool> <passed> <failed> [skipped] [pending] [other]
# Writes a CTRF report (file + stdout `CTRF {...}` marker) and returns non-zero iff failed>0.
emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

# RUN upstream's doctest unit suite (built by mayhem/build.sh into build/tests with normal flags).
# Each test_* binary is run directly and its doctest summary line
#   [doctest] test cases: <T> | <P> passed | <F> failed | <S> skipped
# is parsed; a binary that prints no summary (crashed, or neutered to a no-op) counts as one failure.
# The suite's assertions are round-trip value checks (save -> load -> CHECK_EQ), so it is behavioral.
TESTDIR="$SRC/build/tests/unittests"
mapfile -t BINS < <(find "$TESTDIR" -maxdepth 2 -type f -name 'test_*' -perm -u+x 2>/dev/null | sort)
if [ "${#BINS[@]}" -eq 0 ]; then
  echo "no unit-test binaries under $TESTDIR — mayhem/build.sh did not build the suite" >&2
  emit_ctrf "doctest" 0 1 0
  exit 1
fi

passed=0 failed=0 skipped=0
for b in "${BINS[@]}"; do
  out=$("$b" 2>&1); rc=$?
  line=$(printf '%s\n' "$out" | grep -E '^\[doctest\] test cases:' | tail -n1)
  if [ -z "$line" ]; then
    echo "FAIL $(basename "$b"): no doctest summary (rc=$rc)"; printf '%s\n' "$out" | tail -n 20
    failed=$((failed + 1)); continue
  fi
  p=$(sed -E 's/.*\| *([0-9]+) passed.*/\1/' <<<"$line")
  f=$(sed -E 's/.*\| *([0-9]+) failed.*/\1/' <<<"$line")
  sk=$(sed -nE 's/.*\| *([0-9]+) skipped.*/\1/p' <<<"$line"); sk=${sk:-0}
  if [ "$rc" -ne 0 ] && [ "$f" -eq 0 ]; then f=1; fi
  echo "$(basename "$b"): $line (rc=$rc)"
  [ "$f" -eq 0 ] || printf '%s\n' "$out" | tail -n 40
  passed=$((passed + p)); failed=$((failed + f)); skipped=$((skipped + sk))
done

emit_ctrf "doctest" "$passed" "$failed" "$skipped"
