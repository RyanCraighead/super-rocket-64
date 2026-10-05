#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
out="$(mktemp -d "${TMPDIR:-/tmp}/rocket-pipe-native.XXXXXX")"
trap 'rm -rf "$out"' EXIT
python3 -B codex/rocketleague/tests/extract_pipe_native.py "$out"
cc -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections -fsanitize=address,undefined -fno-sanitize-recover=all -Wall -Wextra -Werror -Wno-unused-variable -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB -I. -Iinclude -Isrc -Isrc/game -Ilib/lua/include -I"$out" "$out/native-audit.c" -Wl,--gc-sections -lm -o "$out/native"
"$out/native"
