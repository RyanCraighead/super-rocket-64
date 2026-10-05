#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-adapter-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
"${CC:-cc}" -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -Wall -Wextra -Werror \
    -I. -Iinclude -Isrc -Ilib/lua/include -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    codex/rocketleague/tests/test_adapter.c -lm -o "$OUT/adapter"
"$OUT/adapter"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror codex/rocketleague/tests/test_vanish_geometry.c -lm -o "$OUT/vanish"
"$OUT/vanish"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror codex/rocketleague/tests/test_gamepad.c -lm -o "$OUT/gamepad"
"$OUT/gamepad"
python3 -m unittest discover -s codex/rocketleague/tests -p test_export.py
# Private assistant CLI tests are not part of the public edition.
