# Windows launcher source preview

This is the standalone Super Rocket 64 launcher and its headless test harness. It contains the versioned payload installer, guided setup UI, argument handling, stable data paths, cancel signaling and read-only LAN/Tailscale address detection.

**This is not a playable release.** The game engine and runtime payload are deliberately absent while the [asset/license publication gate](../docs/RELEASE-STATUS.md) is unresolved. No private repository or private release archive is needed to run these tests.

From Windows PowerShell, with the standard Windows .NET Framework compiler available:

```powershell
./launcher/test-headless.ps1
```

The harness compiles the source using a synthetic payload-info stub. Its 29 checks exercise safe extraction, tampering and malformed archive rejection, save preservation, CLI quoting, setup/online command construction, persistent data paths, optional game formats and Tailscale address classification. It does not show the UI, start a game/helper, configure a network or connect to another machine.

The generated `BootstrapTests.exe` is a test runner, not the game. Production packaging must replace `PayloadInfo.Test.cs` with the audited payload's length/hash and embed that exact payload. UI/native-game testing and the complete packaged setup remain separate acceptance gates.

The launcher source was authored for this project. Engine/dependency licenses and proprietary asset rights are separate and are not granted by this preview. No game assets or third-party executable files are included here.
## Launcher UI checks without taking foreground

```powershell
./launcher/test-desktop.ps1
```

This test compiles the actual launcher into a test executable and starts it on a separate Windows desktop that is never activated. Before creating any form, the child verifies that its desktop differs from the input desktop. It checks 48 navigation, input, online-mode and error-recovery conditions and saves nine page renders under a fresh temporary directory. It does not start a helper/game, discover addresses, inject global input, or change networking. The test runner is not a playable release.

Additional private validation exercised the real embedded runtime through the same UI: missing-input failure, cancel before worker startup, clean owned-input Octane extraction, retry, and reuse with blank source fields while preserving a synthetic save. That run passed 64 UI checks on the separate desktop and never started gameplay. Its private payload and extracted assets are excluded here.

For an existing setup, leave source fields blank and select setup again. The helper validates the cached profile before reusing it; if repair or missing data needs a source, the error identifies it. Supplied paths are still checked. No repeated extraction or tool download is needed for a valid existing profile.
