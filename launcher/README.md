# Launcher tests

The playable Windows release is on the repository's Releases page. Production
launcher source is in `codex/windows/single_exe`; the `Bootstrap.cs` copy in this
folder is retained for existing source references. These scripts compile the
production files with synthetic test payload metadata using Windows' .NET
Framework C# compiler. No private repository or game inputs are required.

```powershell
.\launcher\test-headless.ps1
.\launcher\test-updates.ps1
.\launcher\test-desktop.ps1
```

- 29 baseline checks cover extraction/hash validation, path handling, saved data,
  commands, optional source formats and LAN/Tailscale address classification.
- 22 updater checks cover release selection, offline/no-release responses,
  metadata/version mismatch, corrupted/partial downloads, cancellation, caching,
  operation locks, actual synthetic child probes/restart, rollback and actual
  shortcut targets/removal in isolated temporary folders.
- 85 UI checks exercise first-run choices, saved opt-in, manual checks, prompts,
  disabling checks, cancellation, offline/no-release messages, shortcuts and the
  existing setup/online navigation. The child runs on a new Windows desktop that
  is never activated. No global input is injected. It writes screenshots and an
  isolation report to the temporary output path printed by the script.

The tests do not launch gameplay, change networking or write to the user's real
Desktop/Start Menu. The test-only child executable simulates updater protocol
messages; production package verification and owned-input checks are separate.
The release was also checked with its actual embedded runtime and an existing
data folder. Two-PC/WAN and physical-controller acceptance remain pending.
