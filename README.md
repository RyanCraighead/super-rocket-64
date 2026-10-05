# Super Rocket 64

**Jump, flip, boost and fly through Super Mario 64 in an Octane.**

![Octane flying around Peach's Castle](docs/media/castle-flight.gif)

Launch over the castle moat, powerslide through familiar courses and find new ways to reach the stars. Octane is your starting character, with Mario and an optional cast available from the character wheel.

**[Download for Windows x64](https://github.com/RyanCraighead/super-rocket-64/releases)** · [Supported game files](docs/ASSET-SOURCES.md) · [Report a problem](https://github.com/RyanCraighead/super-rocket-64/issues)

## Features

- RocketSim car movement: throttle, brake, reverse, powerslide, variable-height jumps, directional flips, boost and aerial control.
- Explore SM64 with car interactions for enemies, bosses, doors, caps, platforms and blue coin switches.
- Collect coins for boost: five points per coin, up to 100, with no passive refill.
- Adjustable car speed and jump height, a car-follow camera and customizable controller bindings.
- Play as **Mario and Octane online**, or add Link, Bomberman, Banjo-Kazooie, Spider-Man and Tony Hawk for offline play.

![Octane jumping through Bob-omb Battlefield](docs/media/battlefield-jumps.gif)

## Install and play

1. Download **Super-Rocket-64-Windows-x64.exe** from [Releases](https://github.com/RyanCraighead/super-rocket-64/releases) and open it.
2. Choose **Setup SM64 + Rocket League**. Select your supported SM64 ROM and Rocket League installation using the file and folder pickers.
3. Choose **No** to optional characters, or **Yes** to select the ones you want. Setup asks only for their required files.
4. Choose **Offline** to enter the game as Octane. On safe ground, hold **F7** or **Back / Share** to open the character wheel.

You need your own game data:

- **Super Mario 64:** original US release, 8 MiB, as `.z64`, `.v64`, `.n64`, or a ZIP containing exactly one N64 image.
- **Rocket League for Windows:** an **Epic Games Store or Steam** installation containing `TAGame/CookedPCConsole`. Both stores use the same folder picker. The required packages must match a supported extraction profile; not every update is compatible.
- **Optional characters:** the supported releases of Ocarina of Time, Bomberman 64, Banjo-Kazooie, Spider-Man or Tony Hawk's Pro Skater. See [exact versions, formats and checksums](docs/ASSET-SOURCES.md) before selecting files.

Setup manages Python and extraction tools automatically, validates the files and reuses completed assets. ROMs, extracted assets, saves and settings stay on your PC; they are not included in the download.

## Controls

| Action | Xbox / PlayStation | Keyboard |
| --- | --- | --- |
| Throttle / brake / reverse | RT / LT; R2 / L2 | W / S |
| Steer / air yaw | Left stick horizontal | A / D |
| Air pitch | Left stick vertical | W / S |
| Jump / second jump / flip | A / Cross | L |
| Boost | B / Circle | Comma |
| Powerslide / air roll | X / Square | K with A / D |
| Pause | Start / Options | Space |
| Character wheel | Hold Back / Share | Hold F7 |

Hold jump for more height. Press jump again in the air for a second jump, or add a direction for a flip. To read a sign or talk as Octane, stop upright nearby, face the target and press your jump button; release and press again to advance or close the text.

Change bindings under **Options > Controls > Car Controller**. Generic joysticks may need an SDL controller mapping.

## Make it yours

Custom keyboard, controller and camera settings follow you between Offline, Host and Join and survive updates. Existing custom mappings are recovered automatically on first launch. [Controls and recovery](docs/SHARED-CONTROLS.md).

| Setting | Default | Choices |
| --- | --- | --- |
| **Options > Octane speed (%)** | 75% | 50–100%; 100% restores original speed. Handling and speed-based attacks adjust together. |
| **Options > Octane jump height (%)** | 50% | 50–100%; 100% restores original jump physics. Independent of car speed. |
| **Options > Camera > Octane camera** | Car follow | Car follow or Mario camera; use the normal look/recenter controls. |
| **Options > Octane boost** | Coin only | Coin only or Infinite. |
| **Options > Octane sounds** | Car | Local Rocket League jump/flip/boost sounds, or Mario sounds. |
| **Options > Octane surfaces** | Native surfaces | Native surfaces or Car grip. |

Jump height is approximate: tap and held jumps reach roughly the selected fraction of normal height; second-jump timing and momentum affect total height. For higher ledges, hold jump, double jump, boost or choose 100%. Gravity, boost flight and swim-up stay independent of this setting.

Your choices are saved locally. Online, the host controls speed, jump height, boost and surfaces; joining keeps your offline preferences intact.

## Play together

1. Each player completes setup and uses the **same Super Rocket 64 build**.
2. Choose **Online**, then **Host** or **Join**. Online characters are **Mario and Octane only**.
3. **Host:** choose a port, default **7777**, and share your reachable LAN IP or Tailscale IP. Share your PC's address, not a listen address such as `0.0.0.0`.
4. **Join:** enter the host's IP and matching port.

For Tailscale, configure both PCs outside the launcher and allow them to reach each other. Internet play needs a reachable network route; detecting an address does not guarantee a connection. The launcher never changes firewall, router, VPN or security settings automatically.

## Updates and troubleshooting

Use **Updates & settings** to check for a release, enable optional startup checks or manage Desktop/Start Menu shortcuts. Updates preserve local data and retain the previous launcher for rollback. [Update and removal help](docs/UPDATES.md).

- **Already installed assets?** Leave the source fields blank and run Setup to reuse them. Missing or invalid inputs are reported by name.
- **Rejected game files?** Check the [supported versions and formats](docs/ASSET-SOURCES.md). Renaming a file does not convert it. New or modified Rocket League packages may need a new extraction profile.
- **An optional character failed?** Keep playing with completed characters, or use Back and retry that character after correcting its input.
- **Canceled setup?** Wait for cleanup, then retry. Completed profiles are reused.
- **Missing car sounds?** Select your Rocket League folder in Setup again. Unsupported sound banks fall back to Mario sounds. Car audio currently plays for your own Octane only.

This is a Windows preview fan project. RocketSim approximates Rocket League physics; original paint shaders and complete Rocket League materials are not reproduced. Gameplay and multiplayer bugs are still possible. Include your build, selected character and steps to reproduce when [reporting an issue](https://github.com/RyanCraighead/super-rocket-64/issues).

## Credits and licenses

Created by [Ryan Craighead](https://github.com/RyanCraighead), building on [SM64coopdx](https://github.com/coop-deluxe/sm64coopdx), the SM64 decompilation community, [RocketSim](https://github.com/ZealanL/RocketSim), [Bullet](https://github.com/bulletphysics/bullet3), [UE Viewer](https://github.com/gildor2/UEViewer), [vgmstream](https://github.com/vgmstream/vgmstream), and the character research and conversion work credited in the source. Gameplay footage comes from the creator's captures.

Mario and Super Mario 64 belong to Nintendo. Rocket League and Octane belong to Psyonix/Epic Games. Other games, characters and trademarks belong to their respective owners. This project is unaffiliated with those owners. Retain the included component licenses and attribution; proprietary game content is not licensed or distributed by this project.

[License information](LICENSE.md) · [Contributor credits](credits.txt) · [Third-party notices](codex/windows/THIRD_PARTY_NOTICES.txt)
