# Native tutorial checks

Run `bash codex/rocketleague/tests/test_tutorials.sh` from the repository. It
needs a C compiler, SDL2 development files and Python. ASan/UBSan are enabled.
SDL uses dummy video/audio and an explicitly selected virtual controller.
It does not create a game window, send OS input, use owned assets, or edit saves.

The fixture compiles the actual controller readers, mapper, ASCII/native text
converter and tutorial implementation. Native dialog creation, hook handling,
render state and text pagination functions are extracted verbatim from
`ingame_menu.c`; their audiovisual services and the reported controller family
are explicit fixtures. A separate whole-engine build verifies linkage.

Coverage includes all fifteen bindings across Xbox, PlayStation, Nintendo and
unknown-pad naming, keyboard and punctuation labels, alternate steering,
fresh versus held device activity, disabled/focus gates, six-line pagination,
native A/B advance and closure, zoom cutscene closure, mid-dialog rebinding,
deferred horizontal reflow, shorter-page clamping, long names, classic mode,
unrelated IDs/levels, replaced dialogs, response dialogs and Lua overrides.
The original dialog entry is checked byte for byte after use.
Every stable page is also read through the native renderer and its emitted
glyphs are concatenated. They must exactly match the complete encoded dialogue,
so wrapping cannot silently drop a word or the end of a sentence. This runs for
all binding/device combinations, including both Whomp entry points.

Custom dialogue uses contextual introductions and complete sentences. Explicit
paragraph boundaries keep topics together; the normal glyph wrapper adds as
many six-line pages as needed. Level text omits basic jump/flip input lessons
and unrelated spike warnings, while retaining combat methods, each coin color's
5-point boost grant, the 100-point cap, active boost/surface rules and bosses.
The courtyard alone retains its basic-control and rebinding introduction.

The renderer records each actual native glyph position and asserts its right
edge is inside the native box. The 128-pixel advance budget allows glyph
overhang inside the 143-pixel background. Bounded sections keep related
instructions together; they use more pages for long bindings instead of
reducing the font size. Existing native projection handles window scaling.
`tests/menu-scroll/run.sh` separately covers settings across resolutions and
aspect ratios, controller/keyboard navigation, capture/cancel and persistence.

`.build/tutorial-tests/*.svg` captures four representative pages. Render these
using an isolated headless browser profile if PNG evidence is needed. They
retain native layout/advance measurements but use representative SVG glyphs
and a plain fixture background. They are labeled **source-test renders, not
game screenshots**; do not describe them as in-game visual acceptance.

Mechanic sources for the authored text:

- Existing triggers: courtyard `LakituIntroDialog` (DIALOG_034), BOB entry
  DIALOG_000, WF entry DIALOG_030 and `KingWhompDialog` (DIALOG_114).
- Goomba landing: native `determine_interaction` / `interact_bounce_top`;
  local car position/velocity/freefall bridge in `rocket_adapter.c`.
- Supersonic impact: `rocket_enemy.c` / `physics/enemy_impact.h`.
- All coin colors: `packet_collect_coin.c` and `coin_boost_qa.inc.h` call
  `rocket_runtime_collect_coin` once per accepted pickup; the physical grant
  in `rocket_world_collect_coin` is 5, capped at 100, independent of coin value.
- Surfaces and boost: effective session getters in `rocket_boost.c`.
- Whomp: `rocket_whomp.c`, `physics/whomp_impact.h`, and native
  `king_whomp_on_ground` / `whomp_on_ground`. Four supported wheels on the
  exposed back, a qualifying flip or fast boosted dive enter native damage.
  Native health, attack-cycle cooldown, loot and star progression remain intact.

No trigger, progression, physics, shared-rule or network packet change is
part of the tutorial slice. Hardware controller and real gameplay acceptance
are not claimed, and are not a release gate.
