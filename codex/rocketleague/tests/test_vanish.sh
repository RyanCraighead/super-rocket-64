#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-vanish-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 codex/rocketleague/tests/extract_vanish_native.py "$OUT"
"${CC:-cc}" -std=gnu11 -O1 -Wall -Wextra -Werror -Wno-unused-parameter -fno-fast-math -ffp-contract=off \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    codex/rocketleague/tests/test_vanish_native.c -lm -o "$OUT/native"
"$OUT/native"
