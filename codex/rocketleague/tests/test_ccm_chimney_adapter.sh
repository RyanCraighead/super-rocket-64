#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-chimney-adapter.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
extra=()
if [[ ${1:-} == --baseline ]]; then
    git show "${2:?baseline commit required}:src/game/rocket_adapter.c" > "$OUT/rocket_adapter.c"
    extra+=("-DROCKET_ADAPTER_SOURCE=\"$OUT/rocket_adapter.c\"")
    shift 2
elif [[ ${1:-} == --baseline-file ]]; then
    extra+=("-DROCKET_ADAPTER_SOURCE=\"${2:?exported baseline source required}\"")
    shift 2
fi
"${CC:-cc}" -std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -Wall -Wextra -Werror \
    -fsanitize=address,undefined -fno-omit-frame-pointer \
    -I. -Iinclude -Isrc -Isrc/game -Ilib/lua/include -DVERSION_US -DTARGET_PC -DNON_MATCHING -DAVOID_UB \
    "${extra[@]}" \
    codex/rocketleague/tests/test_adapter.c -lm -o "$OUT/adapter"
if [[ ${1:-} == --expect-baseline-rejection ]]; then
    ulimit -c 0
    for mode in --ccm-body --ccm-wheel; do
        set +e
        "$OUT/adapter" "$mode" > "$OUT/baseline.log" 2>&1
        status=$?
        set -e
        cat "$OUT/baseline.log"
        # A compiler failure or arbitrary crash is not a before-fix proof.
        [[ $status == 134 ]]
        grep -F 'object.numCollidedObjs==count+1&&object.collidedObjs[count]==&chimneyWarp' "$OUT/baseline.log" > /dev/null
        printf 'EXPECTED v4 behavioral failure: %s did not enqueue the native warp\n' "$mode"
    done
else
    "$OUT/adapter" "$@"
fi
