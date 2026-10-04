# Maintaining the public edition

Keep development and public publication separate without copying private history.

1. Record the integrated source revision and packaging revision in a **local** sync record. Preserve the original repository, branches, assets, saves and releases.
2. Review the changes since that revision. Bring only the intended source changes into a staging checkout and keep the public edition's assistant removal, Octane defaults and asset boundaries explicit.
3. Export an allowlisted snapshot into this public repository. Never mirror, merge or push private Git ancestry. Never export ignored runtime folders or build directories.
4. Scan the complete staged tree and release payload, including embedded binary resources. Record file hashes, source/dependency pins, license notices and exclusions. Attribution is not a substitute for permission.
5. Run the relevant regression tests and clean-install checks. Recheck protocol compatibility when networking changes. Test reuse of the same persistent data folder across launcher versions.
6. Publish a normal reviewable commit or pull request here, then a versioned release from that exact audited commit. Keep previous releases intact.

The public build and release must use public source, pinned public dependencies and the user's local game inputs. It must not require access to any private repository or an earlier private release archive. Until that condition and the asset audit are met, keep the game release blocked.

Immutable program versions and mutable user data have separate locations. Updating program files must not replace saves, controller mappings, boost preferences or verified asset profiles. Cached extraction tools are accepted only after their pinned hashes are checked; unknown tools and game builds fail with a specific error.
