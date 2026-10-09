# Fresh-game car welcome

The launcher passes `--skip-intro`, which skips both Peach and Lakitu. Standalone
play uses NT_SERVER as a local save/object-authority marker, without a transport.
Consequently `fake_lvl_init_from_save_file` sets `gNeverEnteredCastle` false on
an unused slot, and Lakitu's initializer deletes him. The prior binding-aware
dialog replacement was unreachable in that shipped startup path.

Without skip-intro, native Lakitu waits on the raised bridge toward the castle
entrance: X between -544 and 545, Y above 800, Z between -2000 and -177. Grounded
car ACT_IDLE can speak normally. Driving there does not restore an NPC already
deleted by the launcher's skip policy.

An unused offline car-capable slot now retains this courtyard Lakitu invisibly.
After the selected car settles upright on dry ground for 15 distinct simulation
frames, with no dialog, cutscene, pause, transition or pending warp, Lakitu
appears and uses his native NPC action, cloud, circling approach, dialog and
departure. The automatic welcome starts his flight nearby and above the player:
the stock bridge-relative origin is over 7700 horizontal units from fresh spawn,
outside the native flight's 5000-unit approach threshold. Other/native intros
keep their original origin and trigger.

Only after this dialog was actually displayed and its native completion returns
does the game commit `SAVE_FLAG_FILE_EXISTS` through the normal checksum/save
path. No new flag, save schema, packet field, star, door, unlock or setting is
introduced. Reloading that slot therefore does not repeat the welcome. Existing
saves receive no automatic welcome or migration. An interrupted or hook-vetoed
dialog does not mark the slot complete. Native backup slots are never written.
Other characters keep the intro-skip policy; character-wheel Mario/other models
also retain original text instead of receiving car-control replacements.

`bash codex/rocketleague/tests/test_welcome.sh` executes verbatim production
startup, Lakitu init/trigger/flight, NPC action, object dialog, camera dialog
dispatch, save flags, checksum and byte-swapped serialization. It uses the exact
courtyard spawn coordinates/yaw and a private temporary EEPROM file. Tests cover
unused A/B/C/D, reload, four headings, other-slot preservation, character switches,
pause/warp/air/water/boost/motion/death gates, frame wrap, Lua veto and interruption.
Explicit fixture boundaries are car pose, scene/audio services, camera event
scheduling, UI rendering and EEPROM transport. Native dialog opening is reached
through the world trigger; it is not injected as a string-only test.

The MinGW build of the same fixture accepts a private temporary-file path on
Windows, whose C runtime otherwise tries to create `tmpfile()` at the drive root:
`$fixture = [IO.Path]::GetTempFileName(); & .build\welcome-tests\welcome.exe $fixture`.
It closes and removes that synthetic EEPROM file after passing.

`test_tutorials.sh` separately exercises actual SDL/keyboard bindings and native
text/render/pagination, including remaps, device families, long names and Lua
precedence. Production presentation/network suites cover the car's native NPC
handoff. These automated tests do not launch a game, touch an installed save,
or claim physical-controller or game-world screenshot acceptance.
