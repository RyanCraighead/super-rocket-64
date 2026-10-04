# Public release status

**No playable public Windows release has been published.**

The public repository currently contains documentation and creator-authorized gameplay previews. A separate local candidate is being prepared from the latest integrated gameplay baseline, with Octane as the default, the character wheel retained, and the assistant integration removed.

## Publication gate

The inherited source provenance records no blanket upstream repository license. Some game-derived tables remain in source, and the existing Windows engine includes built-in/custom graphics and audio. Omitting standalone ROMs and image files from a ZIP does not establish that the executable is free of embedded game content.

Those source files and engine binaries are not included in this public repository. Before a playable release, the embedded-content inventory must be resolved by local input extraction, replacement with cleared content, or documented redistribution permission. Component-specific licenses must be retained and checked against the actual package.

The following remain release gates:

- A public source and binary inventory with documented provenance and no proprietary asset payloads.
- The Windows EXE built from that audited source, with matching hashes and notices.
- Clean-install setup, cancel/retry and asset reuse verification using the bundled runtime.
- Launcher UI and gameplay verification, including the retained character wheel and save/controller persistence.
- Matching-build Mario/Octane multiplayer checks. Optional characters are not supported online.

Older gameplay footage and inherited tests are useful development evidence, not certification of a newly packaged release. Native multiplayer, WAN and physical-controller coverage remain limited.

## Name check

An exact GitHub repository-name search for `super-rocket-64` returned no results before this repository was created. This was an obvious-conflict check, not trademark clearance.
