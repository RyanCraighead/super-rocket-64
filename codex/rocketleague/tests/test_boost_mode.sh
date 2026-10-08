#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rocket-boost-mode-test.XXXXXX")"
trap 'rm -rf "$OUT"' EXIT
FLAGS=(-std=gnu11 -O1 -g -fno-fast-math -ffp-contract=off -ffunction-sections -fdata-sections)
if [[ "${SANITIZE:-1}" == 1 ]]; then FLAGS+=(-fsanitize=address,undefined -fno-sanitize-recover=all); fi
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_boost_mode.c \
    src/pc/rocket_boost.c src/pc/network/version.c src/pc/character_net_codec.c src/pc/character_net.c \
    src/pc/network/packets/packet_read_write.c -Wl,--gc-sections -lm -lz -o "$OUT/rule"
"$OUT/rule"
python3 - "$OUT/surface_join_request.inc.c" <<'PY'
from pathlib import Path
import sys
sys.path.insert(0, 'codex/rocketleague/tests')
from native_slice import function
Path(sys.argv[1]).write_text(function(Path('src/pc/network/packets/packet_join.c').read_text(), 'network_receive_join_request'))
PY
"${CC:-gcc}" "${FLAGS[@]}" -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include -I"$OUT" codex/rocketleague/tests/test_surface_version.c \
    src/pc/network/version.c src/pc/network/packets/packet_read_write.c -Wl,--gc-sections -lm -o "$OUT/surface-version"
"$OUT/surface-version"
"${CC:-gcc}" "${FLAGS[@]}" -D_LANGUAGE_C -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_boost_config.c src/pc/rocket_bindings.c \
    -Wl,--gc-sections -lm -o "$OUT/config"
"$OUT/config" "$OUT"
# Compile the public Windows settings UI and join/init integration (no window).
"${WINDOWS_CC:-x86_64-w64-mingw32-gcc}" -I"${SDL_ROOT:?Set SDL_ROOT to the Windows SDL2 SDK}/include" -std=gnu11 -fsyntax-only -DWINSOCK -D_LANGUAGE_C -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US \
    -I. -Iinclude -Isrc -Ilib/lua/include src/pc/djui/djui_rocket_boost.c \
    src/pc/djui/djui_panel_options.c src/pc/network/packets/packet_join.c src/pc/network/network.c

"${CC:-gcc}" "${FLAGS[@]}" -D_LANGUAGE_C -DNON_MATCHING -DAVOID_UB -DTARGET_PC -DVERSION_US -DDISABLE_MODULE_LOG \
    -I. -Iinclude -Isrc -Ilib/lua/include codex/rocketleague/tests/test_shared_controls_config.c src/pc/rocket_bindings.c \
    -Wl,--gc-sections -lm -o "$OUT/shared-controls"
"$OUT/shared-controls" "$OUT"
