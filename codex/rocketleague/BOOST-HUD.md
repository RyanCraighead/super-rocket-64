# Native-style car boost HUD

The boost display anchors to the bottom-right visible viewport edge using the
same aspect-ratio coordinates as the native star/camera HUD. It retains the
game's full-size colorful HUD font, native coin/percent glyphs and a compact
20-segment gauge; each segment represents five boost. No font shrinking or
replacement font is used. Low finite fuel is red; ordinary fuel is yellow;
underwater jet mode is blue. Infinite/Wing Cap boost and jet mode read MAX.

The camera indicator and arrows move just left of the gauge. Only the Lakitu
head is omitted while this car HUD is present; Mario/fixed camera glyphs remain.
Classic Mario's camera layout and all other native counters/health/timers are
unchanged. Native injury presentation retains the fuel display, while missing
poses and the character wheel hide it. Existing penguin control hints remain
centered above the bottom HUD.

The renderer reads the effective boost mode and snapshot, including temporary
Wing Cap mode, without changing physics, fuel, input, settings or packets.
Finite display values are clamped to 0–100; invalid values fail to empty.

`bash codex/rocketleague/tests/test_boost_hud.sh` compiles the production HUD
function bodies and actual native colorful text queue. It checks every filled
segment, native glyph/label boundaries, finite/infinite/jet modes, injury,
camera modes/arrows, classic Mario fallback and penguin hints across 4:3, 16:9,
16:10, 21:9, 32:9, square and 9:16. The test records drawing commands to
`.build/hud-inspection/*.draw`; fill/icon sinks are explicit fixture boundaries.

Private visual checks render those production commands with original owned
SM64 HUD glyphs plus the game's existing custom percent glyph on a neutral
background. Those captures are isolated HUD visualizations, not a game-world
screenshot or a claim of a completed gameplay session. Proprietary glyph data
and private PNG captures are not part of source or release payloads.
