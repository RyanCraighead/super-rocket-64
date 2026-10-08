#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-bully-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
python3 codex/rocketleague/tests/extract_bully_native.py "$OUT/bully-native.inc.c"
# Native monolithic interaction tables retain unrelated behaviors under ASan.
# UBSan exercises the actual handlers and native stepping without those services.
"${CC:-cc}" -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
    -fsanitize=undefined -fno-sanitize-recover=all -Werror=implicit-function-declaration -Wno-incompatible-pointer-types \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    codex/rocketleague/tests/test_bully.c src/engine/math_util.c src/pc/rocket_bindings.c \
    -Wl,--gc-sections -lm -o "$OUT/bully"
"$OUT/bully"
