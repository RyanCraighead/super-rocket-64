#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
: "${SDL_ROOT:?Set SDL_ROOT to the SDL2 MinGW x86_64 development directory}"
: "${GLEW_ROOT:?Set GLEW_ROOT to the GLEW Windows development directory}"
export SDL_AUDIODRIVER=dummy SDL_VIDEODRIVER=dummy OMP_NUM_THREADS=2 PYTHONDONTWRITEBYTECODE=1
BUILD_JOBS=${BUILD_JOBS:-2}
python3 tools/fetch_lua.py
cmake -S codex/rocketleague -B .build/rocket \
  -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
  -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_BUILD_TYPE=Release
cmake --build .build/rocket --target rocket_physics -j"$BUILD_JOBS"
make -j"$BUILD_JOBS" WINDOWS_BUILD=1 CROSS=x86_64-w64-mingw32- TARGET_BITS=64 TARGET_ARCH=x86-64 \
  BUILD_DIR_BASE=.build/windows DISCORD_SDK=0 COOPNET=0 UPDATER=0 \
  LUA_UNSAFE=0 ICON=0 NO_BZERO_BCOPY=1 ROCKET_CAR=1 ROCKET_CAR_QA=0 \
  ROCKET_CAR_LIBS='.build/rocket/librocket_physics.a .build/rocket/RocketSim/libRocketSim.a' \
  EXTRA_LDFLAGS="-L$GLEW_ROOT/lib" SDLCONFIG="$SDL_ROOT/bin/sdl2-config" \
  EXTRA_INCLUDES="$SDL_ROOT/include $GLEW_ROOT/include"
sha256sum .build/windows/us_pc/sm64coopdx.exe
