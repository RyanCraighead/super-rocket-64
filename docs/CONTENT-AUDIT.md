# Content and attribution

The source/export inventory excludes ROM files, game-installation packages,
extracted user profiles, saves, private repository history and credentials.
Original game textures, samples, mesh/animation/collision data and other local
input assets are loaded or reconstructed from supported owned games at setup.

The runtime retains upstream-style drawing commands, room metadata, camera
scripts and procedural mesh parameters. A numeric match to a ROM does not by
itself classify a program table as a prohibited asset file. The official
SM64coopdx release uses the same general distinction between compiled program
data and its user-ROM asset loader. This is not a claim of blanket rights in
every possible third-party resource.

Original custom upstream artwork and recordings are explicitly inventoried in
[RETAINED-RESOURCES.json](../licenses/RETAINED-RESOURCES.json): 107 PNGs, four
apparition texture arrays and 123 credited custom recordings. Their existing
scope evidence is recorded in [RETAINED-RESOURCE-SCOPE.md](../licenses/RETAINED-RESOURCE-SCOPE.md).
The original-decomp CC0 notice, updated renderer conditions, component terms
and upstream credits are retained. Missing individual notices were not used
to remove characters or invent a blanket prohibition.

The specifically restricted original SGI look-at implementation was replaced;
the separate restricted header is excluded. The compiled dependencies are
listed under `licenses/baseline`; the runtime package also includes CPython's
notices and Unicorn's corresponding source. Discord SDK, CoopNet and updater
binaries are not included in this baseline.
