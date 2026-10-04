# Windows launcher source preview

This is the standalone Super Rocket 64 launcher and its headless test harness. It contains the versioned payload installer, guided setup UI, argument handling, stable data paths, cancel signaling and read-only LAN/Tailscale address detection.

**This is not a playable release.** The game engine and runtime payload are deliberately absent while the [asset/license publication gate](../docs/RELEASE-STATUS.md) is unresolved. No private repository or private release archive is needed to run these tests.

From Windows PowerShell, with the standard Windows .NET Framework compiler available:

```powershell
./launcher/test-headless.ps1
```

The harness compiles the source using a synthetic payload-info stub. Its 28 checks exercise safe extraction, tampering and malformed archive rejection, save preservation, CLI quoting, setup/online command construction, persistent data paths, optional game formats and Tailscale address classification. It does not show the UI, start a game/helper, configure a network or connect to another machine.

The generated `BootstrapTests.exe` is a test runner, not the game. Production packaging must replace `PayloadInfo.Test.cs` with the audited payload's length/hash and embed that exact payload. UI/native-game testing and the complete packaged setup remain separate acceptance gates.

The launcher source was authored for this project. Engine/dependency licenses and proprietary asset rights are separate and are not granted by this preview. No game assets or third-party executable files are included here.