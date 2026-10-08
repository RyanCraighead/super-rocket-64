# Native penguin roof carry

Park upright facing a nearby baby penguin and press the current Boost binding
(default Circle/B, or the keyboard Attack/Boost binding). Release it after pickup.
The context hint displays the saved controller and keyboard/mouse bindings.
While parked, another press sets the penguin down in front of the car when the
path is clear. A blocked set-down releases it from the roof instead.

Normal driving, boost, jumps, and flip controls remain available. A flip, a strong
roll, water entry, injury, death, or area transition drops the actual actor. It can
be picked up again. Carry the correct upper penguin to its mother and finish the
native dialogue. The mother still compares the baby parameters, requests its
release, and spawns the star after the native release transition. The wrong baby
still receives the wrong-baby dialogue and no star.

There is one native actor and native held-object identity. No new save field,
binding format, quest flag, network message, or physics parameter is introduced.
An ordinary held item still suspends car control. Only verified roof carry passes
the existing car contact gates; character switching still requires setting down
the held item.

## Automated checks

Run `bash codex/rocketleague/tests/test_penguin.sh`. It uses ASan/UBSan and actual
adapter, grab/drop, mother quest, held actor rendering, and player packet code.
World physics, animation/audio, and transport services are explicit test
boundaries. The packet fixture also reruns the existing transport checks.

Run the existing `test_host.sh`, `test_bindings.sh`, `test_network.sh`,
`test_enemy.sh`, `test_whomp.sh`, `test_player_bump.sh`, and `test_platforms.sh`
alongside it. These tests do not open the game or use live controller input.

## Post-release manual acceptance

- In CCM, pick up the upper baby using keyboard and a controller, drive/jump down
  the mountain, finish the mother's dialogue, and collect the native star.
- Confirm the lower wrong baby still gives the wrong response and no star.
- Check roof clearance and animation on slopes; flip/drop, re-pickup, and set down
  beside a wall. Check a saved remapping in the on-screen hint.
- With two compatible peers, check remote roof position and ownership handoff,
  then leave/rejoin the area. Check damage, death, and warp cleanup.

This checklist is acceptance coverage after publication, not a manual gameplay
approval gate. Automated fixture renders must not be described as full gameplay
screenshots.
