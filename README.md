# Seeker Fix

Seeker Gun and HK-22 missiles no longer back away from their target while they lock on, so they stop blowing up on walls and floors. The lock-on delay is kept.

By Slim Cognito. For Ratchet & Clank: Going Commando in OpenRAC: NTSC-U v1.01, NTSC-U v2.00 (Greatest Hits) and PAL (*Locked and Loaded*).

## Building

```sh
python3 tools/fetch_sdk.py          # the OpenRAC mod SDK sdk.lock pins, into .sdk/
cmake -S . -B build -G Ninja
cmake --build build
python3 .sdk/openrac-mod-sdk-*/tools/package_mod.py build/package/seeker_fix -o dist
```

`lib/windows-x86_64/` holds a Windows build made without the C runtime
(`clang --target=x86_64-pc-windows-msvc -ffreestanding -DOPENRAC_FREESTANDING`,
`lld-link /dll /noentry /nodefaultlib /Brepro`), so the folder can be added to the
launcher as it is.

## Releasing

`.github/workflows/build.yml` is the OpenRAC mod SDK's template workflow (as in
openrac-mods). Every push and pull request checks and packages the mod; a tag
`v<version>` (the manifest's version) publishes the archives and `SHA256SUMS`
on that release, which the launcher installs from when a player pastes the
repository's address.
