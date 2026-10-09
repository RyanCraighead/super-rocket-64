# Rear boost visual

The car previously rendered its body and wheels without an exhaust effect.
It now draws two soft, animated orange flames when the snapshot's existing
`boosting` flag reports actual backend thrust. Fuel amount or held input alone
does not trigger the effect. The same renderer handles local and remote cars;
no packet fields, physics, fuel, controls or settings change.

The effect uses the owned Octane's `BoostEmitterLeft` / `BoostEmitterRight`
socket locations: local forward/right/up `(-48,-9,9)` and `(-48,9,9)`.
`MasterBoost_Standard_MIC` specifies CustomColor `(1,0.20867,0.0697254)`.
Those data were inspected before implementation, along with the owned Standard
boost thumbnail, smoke and gradient textures. No AI assets are used.

This is an original host flame implementation, not Rocket League's UE3 particle
animation or material graph. The pinned reader cannot load FXActor_Boost_TA or
BoostMesh_TA classes; its thumbnail is a reference, not an animation capture.
Crossed soft ribbons, glow, pulse rate and length are host approximations.
No owned textures, meshes, extracted binaries or proprietary code ship in this
change. Existing optional car diffuse textures are independently supported.

Evidence profiles (local inspection only):
- Body_Octane_SF.upk SHA-256
  `bedf7fc0d64ab2c6d4f2620a946bb88e600f779c3e924fb80ab96c431d67f7e6`.
- Boost_Standard_SF.upk: 81,400 bytes, SHA-256
  `2c393f4117bdadbdd0c4e68ca2a7901a22c8ecbc85bea3b9d8bac7c0067c9485`.
- Boost_Standard_T_SF.upk: 177,179 bytes, SHA-256
  `e774934b2af7d173794ccc2391985cc3ff8151ec5f7226f20d3493a117708cef`.
- Pinned UEViewer and temporary body/Startup header compatibility copies use
  the existing verified materials workflow; installed files remain unchanged.

The effect has no persistent particles or draw-time clock. Bounded snapshot
ticks control its pulse; repeated draws and paused poses remain identical.
Release/exhaustion immediately removes it on the next non-boosting snapshot.
Native injury, doors, poles and cutscenes clear only the presentation copy's
thrust flag, preserving fuel and the actual backend state. Warps and character
changes cannot leave an old trail. No source animation is invented for native
locomotion. Quicksand and squish transforms apply to both body and nozzles.

The depth-tested additive pass writes no depth, respects native Vanish stipple,
and keeps exhaust orange under Metal. The existing GL state guard restores
blend factors/equations, depth mask, texture, program, attributes and viewport
before native HUD or other characters render.

Validation:
- `bash codex/rocketleague/tests/test_boost_visual.sh`: production geometry,
  two full animation cycles, socket bounds, 16 rotated/transformed poses,
  deterministic replay, actual-thrust gating and invalid input rejection.
- Same geometry fixture compiled with MinGW and executed on Windows.
- `bash codex/rocketleague/tests/test_material_render.sh`: actual production
  OpenGL 3 / 2.1 / GLES 2 offscreen draws, idle/thrust, frame changes, world
  occlusion, Metal/Vanish and hostile GL state restoration.
- Optional owned geometry/material arguments produce private before/after
  captures. These are actual renderer fixtures, not game-world screenshots.
- `test_presentation_bumps.sh`, `test_world_draw.sh`, and production shader
  compile/link validation cover native transitions, packet round trips and HUD.
