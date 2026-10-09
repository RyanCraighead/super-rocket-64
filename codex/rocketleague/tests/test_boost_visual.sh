#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
mkdir -p .build/boost-visual
g++ -std=c++17 -Wall -Werror -fsanitize=undefined -I. codex/rocketleague/tests/test_boost_visual.cpp -o .build/boost-visual/test
.build/boost-visual/test
