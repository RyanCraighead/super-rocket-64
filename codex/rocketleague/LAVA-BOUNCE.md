# Car lava contact and escape

The native floor check expects Mario's feet within ten units of the floor.
Octane mirrors its chassis origin and handles its own landing, so a car could
drive on lava without entering the native burn action. Once burning, the native
entry erased forward speed and SDL's car controls were isolated from native
Mario analog movement, causing repeated vertical bounces.

The bridge queries native burning floors at actual contacting tires and the
oriented Octane body corners. Hovering tires, missing floors, stale/foreign car
poses, unsupported actions and paused/frozen ownership cannot grant contact.
It passes the result to the existing native floor burn handler. That handler
keeps its hazard-hook veto, Metal-cap damage rules, held-object drop and native
lava action. No damage loop, timer exemption or save field is added.

On entry, verified local car motion supplies a 16..32 native units/frame escape
speed, retaining travel direction. A stationary floor hit launches forward;
native wall burn retains its outward heading. During ACT_LAVA_BOOST only,
mapped accelerator/brake changes forward speed by at most one native unit/frame,
steering changes yaw by at most 512 angle units/frame, and speed is capped at 32.
Neutral controls retain native .35 coast deceleration. Boost/jump/air-roll do
not supply force. Keyboard and saved gamepad mappings use the same selection,
focus and UI gates even while car rendering/physics ownership is suspended.

The native air quarter-steps, gravity, vertical impulse of 84, wall response,
repeat landing damage, safe-ground landing bounces, health/death hooks and bubble
or death warp remain unchanged. Ordinary Mario uses its existing movement.
No car physics constants, network format, save format or asset profiles change.

`bash codex/rocketleague/tests/test_lava.sh` passes 140 checks using verbatim
production floor handling, action transitions, native air quarter-steps,
gravity and lava action. Tests include initial drive/fall contact, roof contact,
hover/no-floor rejection, tires across a safe/lava boundary, no per-frame damage,
hook veto, Metal damage protection, capless damage, forward/reverse/wall entry,
native flight to a safe shore, repeat native lava bounce, bounded control,
focus/pause/freeze/remote gates, keyboard, native HP drain to death, protected
Metal bounces and damage resuming after expiry, and native death/bubble handling.
An additional fixture compiles the production runtime input-reader bodies and
actual saved-binding mapper for drawable suspension, focus/UI, remapping and
disconnect. Both use ASan/UBSan. Geometry/input/audio are explicit boundaries;
these are source integration tests, not full-game visual/controller acceptance.
