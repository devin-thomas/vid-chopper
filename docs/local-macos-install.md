# macOS 1.2.0 Installation

VidChopper 1.2.0 supports Apple Silicon Macs running macOS 15 or newer. The
public release remains gated until the exact candidates pass both physical
Macs; until then, use retained candidate artifacts only for qualification.

## Requirements

- macOS arm64 with a macOS 15 SDK or newer.
- CMake 3.28 or newer, Ninja, Apple Clang, and Qt 6.7 or newer.
- The repository vcpkg manifest bootstrapped at `.vcpkg`.
- External `ffmpeg` and `ffprobe` 6.1 through major 9.x. They are never bundled.

The current qualification Mac used macOS `26.5.2`, Apple Silicon, CMake
`4.4.2`, Qt `6.11.1`, and FFmpeg/ffprobe `9.0.1`.

## Install the release DMG

Download the DMG and its adjacent `.sha256` file from the same GitHub release.
Verify the bytes before opening the image:

```sh
shasum -a 256 -c VidChopper-1.2.0-macos-arm64.dmg.sha256
```

Open the DMG, drag `VidChopper.app` to the Applications link, eject the image,
and launch VidChopper from `/Applications`.

The application is ad-hoc signed. It is not Developer ID signed or notarized.
For a browser-downloaded copy, macOS may require a one-app approval: try to
open VidChopper once, then open System Settings > Privacy & Security and choose
Open Anyway for VidChopper. Confirm the named app in the macOS dialog and
relaunch it from Applications. Do not disable Gatekeeper globally, run a global
`spctl` command, or remove quarantine metadata.

## Build and install from source

From the repository root:

```sh
./script/build_and_run.sh --verify
```

The script configures and builds the `macos-gui-release` preset, runs CMake's
Qt deployment step, installs `VidChopper.app` at
`~/Applications/VidChopper.app`, and opens it. `--no-launch` builds and stages
the app without opening it. The `Run` action in Codex is wired to the same
script through `.codex/environments/environment.toml`.

The app bundle reports version `1.2.0`, contains the VidChopper icon and
metadata, and uses arm64 binaries with local Qt paths removed from the
installed executable's link set. The local install is ad-hoc signed after
deployment so its nested framework signatures are consistent; it is not
notarized and does not identify a developer.

## Standalone CLI

For the published CLI archive, verify the adjacent checksum and extract it:

```sh
shasum -a 256 -c VidChopper-1.2.0-macos-arm64-cli.tar.gz.sha256
tar -xzf VidChopper-1.2.0-macos-arm64-cli.tar.gz
./VidChopperCLI/VidChopperCLI --version
```

Double-click `Install CLI.command` or run it from Terminal. It installs
`~/.local/bin/vidchopper` without `sudo`, preserves an existing command as a
timestamped previous copy, and prints PATH guidance without modifying shell
startup files.

To build the same package shapes from source:

```sh
./tools/package-macos-cli.sh --install
./tools/package-macos-app.sh --sign --cli dist/macos/VidChopper-1.2.0-macos-arm64-cli.tar.gz
```

The CLI is installed as `~/.local/bin/vidchopper`; add that directory to
`PATH` if the shell does not already include it. The disk image is written to
`dist/macos/` with an adjacent SHA-256 checksum. `--sign` means ad-hoc signing
only; it does not notarize the image or bypass Gatekeeper.

## External video tools

Install and review `ffmpeg` and `ffprobe` separately. VidChopper discovers
them through configured paths, `PATH`, and the documented macOS defaults. The
1.2.0 Auto encoder path tests HEVC VideoToolbox on Apple Silicon and falls back
to x264 before export if the capability test fails. An explicit hardware
encoder failure remains visible and does not silently change the preference.

## Support boundary

The 1.2.0 release supports the GUI and standalone CLI on Apple Silicon only;
Intel Macs are not qualified. Windows 10/11 x64 remains supported by the
cumulative Windows ZIP. Linux remains source- and CI-compatible but has no
supported end-user package until 1.3.0.
