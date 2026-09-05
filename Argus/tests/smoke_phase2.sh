#!/usr/bin/env bash
# Phase 2 smoke test: sample image, launch stability, self-check verdict.
# Run from anywhere: ./tests/smoke_phase2.sh (resolves to Argus/ itself).
# Needs only bash and the built binary. No test framework.
set -u

cd "$(dirname "$0")/.." || exit 1
FAILURES=0

check() {
    if eval "$2"; then
        echo "PASS: $1"
    else
        echo "FAIL: $1"
        FAILURES=$((FAILURES + 1))
    fi
}

# Portable bounded run: GNU timeout, else background plus sleep and kill.
run_bounded() {
    if command -v timeout >/dev/null 2>&1; then
        timeout "$1" "${@:2}"
        return $?
    fi
    "${@:2}" > smoke_phase2.log 2>&1 &
    RUN_PID=$!
    sleep "$1"
    if kill -0 "$RUN_PID" 2>/dev/null; then
        kill "$RUN_PID" 2>/dev/null
        wait "$RUN_PID" 2>/dev/null
        return 124
    fi
    wait "$RUN_PID"
    return $?
}

check "sample image present" \
    '[ -f resources/images/car_01.jpg ]'
check "app binary present" \
    '[ -x bin/Argus ]'

run_bounded 8 ./bin/Argus > smoke_phase2.log 2>&1
RUN_STATUS=$?
check "app stays open (no instant crash)" \
    "[ $RUN_STATUS -eq 124 ]"
check "self-check verdict OK" \
    'grep -q "Phase 2 tests: OK" smoke_phase2.log'
rm -f smoke_phase2.log

if [ "$FAILURES" -ne 0 ]; then
    echo "$FAILURES check(s) failed."
    exit 1
fi
echo "All Phase 2 smoke checks passed."
