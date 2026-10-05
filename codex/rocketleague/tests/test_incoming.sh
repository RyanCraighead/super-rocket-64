#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-incoming-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-cc}" "${FLAGS[@]}" -Wall -Wextra -Werror codex/rocketleague/tests/test_body_contact.c -lm -o "$OUT/body"
"$OUT/body"
"${CC:-cc}" "${FLAGS[@]}" -Wall -Wextra -Werror \
    -I. -Iinclude -Isrc -Ilib/lua/include -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    codex/rocketleague/tests/test_incoming_collision.c -Wl,--gc-sections -lm -o "$OUT/collision"
"$OUT/collision"
for fixture in native bobomb; do
    # Native monolithic tables retain all unrelated behaviors with ASan global
    # registration. Keep UBSan here; body geometry/lists above retain ASan too.
    "${CC:-cc}" "${FLAGS[@]}" -fno-sanitize=address -Werror=implicit-function-declaration -Wno-incompatible-pointer-types \
        -I. -Iinclude -Isrc -Ilib/lua/include -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
        "codex/rocketleague/tests/test_incoming_$fixture.c" src/engine/math_util.c \
        -Wl,--gc-sections -lm -o "$OUT/$fixture"
    "$OUT/$fixture"
done
