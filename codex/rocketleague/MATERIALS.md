# Original car materials

The legacy `octane-host-mesh-v1` cache contains positions, normals and two flat
colors. It omits UVs, so it paints the windows blue and loses chassis detail.
Existing geometry hashes and cache files are unchanged by this feature.

An optional `octane-model/materials` directory restores the original first UV
set and owned body/chassis diffuse textures. The source material instances
`MIC_Body_Octane` and `OctaneChassis_MIC` reference `Pepe_Body_D` and
`Chasis_Pepe_D`, respectively. The first 48,480 body vertices use the chassis
texture; the next 36,954 use the paint/window/trim texture. Original UV order is
verified against the complete decoded legacy geometry before accepting it.

Select the installed Rocket League folder in Setup to add these materials to a
verified cached mesh. This upgrade requires the supported body and Startup
packages, but does not re-extract the wheel package. The package and decoded
output hashes are pinned in `codex/windows/rocket-material-profile.json`.
The pinned UE Viewer runs in hidden export mode against temporary compatibility
copies. Original installed files, mesh cache, sounds, saves and controls remain
unchanged. Missing caches, unsupported versions, failed extraction or invalid
materials keep the legacy car playable. Cancellation preserves existing data;
a successfully replaced invalid material directory is retained for recovery.
Setup refuses to add materials while its game launch lock exists.

The renderer samples the original textures and adds compact host lighting and
view-dependent highlights. Blue paint, dark opaque windows and trim remain
distinct. The original UE3 shader graph, environment reflections, normal maps,
paint finishes, transparent interior and renderer parity are not reproduced.
Wheels retain the existing geometry/color treatment. Metal Cap uses the native
host environment textures over every car part; Vanish Cap retains its stipple.
Physics, authority, snapshots and network formats are unchanged.

Validation commands:
- `python codex/windows/tests/test_car_materials.py`
- `bash codex/rocketleague/tests/test_material_render.sh`
- `python codex/rocketleague/tests/test_metal_shaders.py`

The renderer test uses only SDL's offscreen backend, software rendering and
synthetic geometry/textures by default. Supplying a verified model directory,
material directory and screenshot prefix enables private comparisons using the
actual production renderer. It never starts the game, creates a physics world,
opens an audio device or touches a running session. Comparison images show an
offscreen fixture, not in-game gameplay. Decoded assets and images stay private.
