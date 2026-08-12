# Build from Source for the 1.1.0 Foundation

This guide is for source and CI qualification. `1.1.0` publishes Windows 10/11 x64 binaries only.
macOS and Linux commands below establish native core/CLI builds and GUI compile/launch smoke evidence;
they do not describe supported Unix installation or public Unix packages.

## Prerequisites

- CMake 3.28 or newer.
- A C++20 compiler: MSVC 2022 on Windows, Apple Clang on macOS, or GCC/Clang on Linux.
- The repository-pinned vcpkg baseline from `vcpkg.json`, with `nlohmann-json` and `yaml-cpp` installed
  through the manifest.
- Qt 6.9 for GUI compilation. The GUI smoke lane supplies the appropriate offscreen environment.
- External `ffmpeg` and `ffprobe`. The foundation contract supports versions 6.1 through major 8.x;
  neither tool is bundled or auto-installed.

The full GUI build is optional for the core/CLI source lane. Keep the checkout and all generated build
directories local.

## Local macOS 1.2.0 candidate

The unpublished macOS arm64 candidate has a project-local build and install entrypoint. On the current
Apple Silicon Mac, bootstrap the prerequisites above and run:

```sh
./script/build_and_run.sh --verify
```

This installs the deployed bundle at `~/Applications/VidChopper.app` and launches it. Use
`./script/build_and_run.sh --no-launch` to stage without opening the app. For the full local workflow,
including the optional CLI install and disk-image checksum, see [Local macOS 1.2.0 Candidate](local-macos-install.md).

This is a local development candidate only. It does not change the `1.1.0` public support boundary,
publish a release, or bundle `ffmpeg`/`ffprobe`.

## Bootstrap dependencies

On Windows, use the repository bootstrap and set `VCPKG_ROOT` in the same PowerShell session:

```powershell
pwsh -NoProfile -File tools/bootstrap.ps1
$env:VCPKG_ROOT = (Resolve-Path .vcpkg).Path
```

On macOS or Linux, use the upstream vcpkg bootstrap script and the repository manifest:

```sh
git clone https://github.com/microsoft/vcpkg.git .vcpkg
./.vcpkg/bootstrap-vcpkg.sh -disableMetrics
export VCPKG_ROOT="$PWD/.vcpkg"
"$VCPKG_ROOT/vcpkg" install --x-manifest-root="$PWD"
```

The checked-in `vcpkg.json` remains the dependency source of truth. Do not replace its baseline with a
floating system package or a second dependency manager.

## Core and CLI

Configure a clean, Qt-free build with the repository toolchain file. Use the shell syntax for the target
platform.

Windows PowerShell:

```powershell
cmake -S . -B build/foundation-core `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVIDCHOPPER_BUILD_CLI=ON `
  -DVIDCHOPPER_BUILD_GUI=OFF `
  -DVIDCHOPPER_BUILD_TESTS=ON
cmake --build build/foundation-core --config Release
ctest --test-dir build/foundation-core -C Release -L fast --output-on-failure
```

macOS and Linux:

```sh
cmake -S . -B build/foundation-core \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVIDCHOPPER_BUILD_CLI=ON \
  -DVIDCHOPPER_BUILD_GUI=OFF \
  -DVIDCHOPPER_BUILD_TESTS=ON
cmake --build build/foundation-core --config Release
ctest --test-dir build/foundation-core -C Release -L fast --output-on-failure
```

The CLI executable is `VidChopperCLI.exe` on Windows and `VidChopperCLI` on Unix-like build hosts. The
CLI flag surface is defined by `src/cli/cli_arguments.cpp`; use `--help` from the built executable rather
than assuming a future flag exists.

## GUI compile and smoke evidence

With Qt 6.9 installed, configure a GUI build by pointing CMake at the Qt installation that contains
`lib/cmake/Qt6`:

```sh
cmake -S . -B build/foundation-gui \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DQt6_ROOT="$QT6_ROOT" \
  -DVIDCHOPPER_BUILD_CLI=ON \
  -DVIDCHOPPER_BUILD_GUI=ON \
  -DVIDCHOPPER_BUILD_TESTS=OFF \
  -DVIDCHOPPER_BUILD_QT_TESTS=ON
cmake --build build/foundation-gui --config Release
ctest --test-dir build/foundation-gui -C Release -L qt --output-on-failure
```

On Windows, use the same CMake definitions in an x64 Native Tools PowerShell session. The Unix CI lanes
add their platform-appropriate offscreen setting, start the GUI, wait for the ready marker, and terminate
cleanly. Do not add a new GUI command-line flag to reproduce that smoke test; the lane wrapper is the
source of truth.

A successful macOS or Linux GUI compile/smoke lane is evidence that the shared shell can build and start
in the qualification environment. It is not a packaged application, an installation path, or an
end-user support promise for `1.1.0`.

## Tool and encoder checks

Run the external tools separately before a real media test:

```sh
ffmpeg -version
ffprobe -version
```

The resolver checks an explicit configured executable first, then `PATH`, common Homebrew locations such
as `/opt/homebrew/bin` and `/usr/local/bin`, and standard Unix locations such as `/usr/local/bin` and
`/usr/bin`. It normalizes duplicate candidates, verifies executability, runs `-version`, parses the
version, and blocks versions below 6.1 or at major 9 and above. A supported ffmpeg/ffprobe version
mismatch remains a visible warning with both paths and versions.

Auto encoder selection must use a real minimal capability encode before hardware export. Auto chooses
usable HEVC VideoToolbox on Apple Silicon only in the `1.2.0` contract; Windows/Linux use usable HEVC
NVENC when available; all other cases resolve to x264. A failed Auto capability test records the reason
and falls back to x264 before export. An explicit hardware failure blocks export and never silently
changes the stored preference.

## Evidence boundary

Record the source commit, OS/version/architecture, compiler, Qt version, FFmpeg and ffprobe paths and
versions, lane outcome, and any failure or retest note in the [1.1.0 foundation evidence record](1.1.0-foundation-evidence.md).

This page intentionally omits macOS installation, Linux package installation, release-workflow changes,
and packaging version metadata. Those are outside this documentation branch; Windows candidate packaging
and publication ownership remains with VCU-111.
