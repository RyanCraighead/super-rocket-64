# Supported source games

These are the exact profiles accepted by the existing conversion code. The first Windows release verifies these identities during setup. Ownership alone does not make a different region, revision, patch or packaging format compatible.

| Character | Required source | Accepted input | Identity after byte-order normalization |
| --- | --- | --- | --- |
| Mario / base world | Super Mario 64, original US release, 8 MiB | `.z64`, `.v64`, `.n64`, or ZIP containing exactly one N64 image | SHA-1 `9bef1128717f958171a4afac3ed78ee2bb4e86ce` |
| Octane | Rocket League Windows installation, **Epic Games Store or Steam**, matching the package profile below | Installation directory containing `TAGame/CookedPCConsole`; two exact packages below | SHA-256 package checks |
| Link | The Legend of Zelda: Ocarina of Time, US 1.2 compressed retail | Raw `.z64`, `.v64`, or `.n64`; no ZIP | SHA-1 `41b3bdc48d98c48529219919015a1af22f5057c2` |
| Bomberman | Bomberman 64 (1997), USA 1.0, 8 MiB | Raw `.z64`, `.v64`, or `.n64`; no ZIP | SHA-1 `8a7648d8105ac4fc1ad942291b2ef89aeca921c9` |
| Banjo-Kazooie | Banjo-Kazooie, USA Rev 1, 16 MiB | `.z64`, `.v64`, `.n64`, or ZIP containing exactly one N64 image | SHA-1 `ded6ee166e740ad1bc810fd678a84b48e245ab80` |
| Spider-Man | Spider-Man, N64 USA 1.0, 32 MiB | Big-endian `.z64`, or ZIP containing that single image | SHA-256 `feff90ed1201c91ff167d66958048e61c192c9d6a756ddb98f799017ac9cd25c` |
| Tony Hawk | Tony Hawk's Pro Skater, N64 USA Rev 1, 12 MiB | Big-endian `.z64`, or ZIP containing that single image | SHA-256 `506961e65197aaff2b47ae055b7d28470f80fb925914ebeac9ddc541a7a9fd8d` |

Extensions are hints; headers, size and checksums determine acceptance. ZIP input is supported only where explicitly listed. Patched, decompressed, wrong-region and unsupported revisions are rejected. Optional characters need only their own listed source once the base SM64 input is installed. They are offline only.

## Rocket League profile

Both stores use the installation root containing `TAGame/CookedPCConsole`. Use the same Rocket League **Browse** folder picker for either store. It does not need a Steam account or running Steam client. Package contents, not the store or installation folder name, determine compatibility.

The exact package pair below was tested automatically from Steam build **25535926**. Matching Epic files use the same extraction path. On **October 5, 2026 at 01:31 UTC**, Ryan reported that he tested the Epic installation and it worked. Epic installation is therefore **user-verified**; the automated coverage consists of Steam-input extraction and shared folder-selection checks for both store layouts. The report did not specify an Epic build number or supply a separate automated test log. Future Epic or Steam updates may change these hashes.


| File | SHA-256 |
| --- | --- |
| `TAGame/CookedPCConsole/Body_Octane_SF.upk` | `bedf7fc0d64ab2c6d4f2620a946bb88e600f779c3e924fb80ab96c431d67f7e6` |
| `TAGame/CookedPCConsole/wheel_sport80_SF.upk` | `9b2582f69e6bf2fd06272b9b545dfd33cfc931f31d373b63a1078a747d902560` |

Missing files and unknown package hashes are reported before extraction tools are provisioned or run. Unknown package hashes need a new compatible extraction profile. Maintainers can collect just package sizes/hashes with `python codex/rocketleague/tools/export_octane.py --game "C:\Program Files\Epic Games\rocketleague" --inspect-only`; this does not extract assets or launch a game. Players do not need to install Python for normal setup. Setup does not alter the installed game. It repairs the known chunk-table difference only in a temporary copy, exports selected objects, and validates the converted geometry. Original paint shaders and complete Rocket League materials are not reproduced.

The pinned extractor is [UE Viewer](https://github.com/gildor2/UEViewer/tree/a0bfb468d42be831b126632fd8a0ae6b3614f981), commit `a0bfb468d42be831b126632fd8a0ae6b3614f981`, build 1590. The executable SHA-256 is `13502e5a4d8f6b5f32252afebd6360f7302ccfaccf6b8dda65beff0be2d364a0`. The setup flow verifies the tool and its supporting files before execution and checks cached copies again before reuse.

No ROM or proprietary asset downloads are provided. Keep installed asset folders and saves private.

## Optional Octane event audio

Setup also checks `SFX_Car_Movements.bnk` and `SFX_Boost_Standard.bnk` under the same `TAGame/CookedPCConsole` directory. The exact accepted bank hashes, event IDs and six decoded WAV hashes are recorded in [`rocket-audio-profile.json`](../codex/windows/rocket-audio-profile.json). These banks were decoded and verified from the owned Steam 25535926 installation. Matching Epic banks follow the same content-checked path; the separate Epic geometry acceptance report above did not test these new sounds. Unknown sound banks leave the car playable with Mario sounds and report which optional bank needs an updated profile.

The decoder is the original [vgmstream r2117 Windows x64 release](https://github.com/vgmstream/vgmstream/releases/tag/r2117). Its archive SHA-256 is `6c4a8a3813864fefed081bbd337dbc0ad93bf88e0b92f5db98d7ab258b22dc6c`; the executable and supporting file hashes are pinned in `vgmstream-r2117.json`. Setup verifies the original download, every extracted tool file, the input banks and every decoded clip. It retains the original COPYING/README/USAGE files with the local tool. Valid cached tools and audio are reused; invalid audio is backed up only after a complete replacement validates. Canceling or failing extraction does not discard the existing geometry or save profile.

The local result contains one representative stereo variant each for ground jump, directional flip and double jump, plus the standard boost start, loop and end clips. Event routing was checked against the owned banks; this is not a recreation of all Wwise randomization, spatialization or processing. Runtime verifies the WAV hashes again and mixes them into the existing output. No sound bank, WEM/WAV file or decoder binary is included in the public source/release.
