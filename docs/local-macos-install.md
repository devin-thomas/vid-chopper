# Local macOS 1.2.0 Candidate

This checkout contains the unpublished `1.2.0` macOS candidate for local use on
Apple Silicon. It is built and installed on the current Mac; it is not a GitHub
release, notarized package, or support claim for the public `1.1.0` boundary.

## Requirements

- macOS arm64 with a macOS 15 SDK or newer.
- CMake 3.28 or newer, Ninja, Apple Clang, and Qt 6.7 or newer.
- The repository vcpkg manifest bootstrapped at `.vcpkg`.
- External `ffmpeg` and `ffprobe` 6.1 through major 8.x. They are never bundled.

The current qualification Mac used macOS `26.5.2`, Apple Silicon, CMake
`4.4.2`, Qt `6.11.1`, and FFmpeg/ffprobe `8.1.2`.

## Build and install

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

## CLI and disk image

The Qt-free CLI can be installed locally and packaged for controlled hand-off:

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

## Scope boundary

This local candidate intentionally does not push a branch, create a tag, alter
the published release, or claim physical qualification on another Mac model.
The remaining roadmap work for hosted CI and release publication stays in
Linear for a later decision.
