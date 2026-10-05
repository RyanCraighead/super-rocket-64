#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-pipe.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections \
 -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror \
 -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -I. -Iinclude -Isrc -Isrc/game -Ilib/lua/include \
 codex/rocketleague/tests/test_pipe_entry.c -Wl,--gc-sections -lm -o "$OUT/pipe"
"$OUT/pipe" "$@"
