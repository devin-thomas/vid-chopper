# Build VidChopper 1.2.0 from Source

This guide covers supported Windows and Apple Silicon macOS builds plus Linux source/CI qualification.
The 1.2.0 release does not publish or support an end-user Linux package.

## Prerequisites

- CMake 3.28 or newer.
- A C++20 compiler: MSVC 2022 on Windows, Apple Clang on macOS, or GCC/Clang on Linux.
- The repository-pinned vcpkg baseline from `vcpkg.json`, with `nlohmann-json` and `yaml-cpp` installed
  through the manifest.
- Qt 6.9 for GUI compilation. The GUI smoke lane supplies the appropriate offscreen environment.
- External `ffmpeg` and `ffprobe`. The 1.2.0 contract supports versions 6.1 through major 9.x;
  neither tool is bundled or auto-installed.

The full GUI build is optional for the core/CLI source lane. Keep the checkout and all generated build
directories local.

## Apple Silicon macOS

On an Apple Silicon Mac, bootstrap the prerequisites above and run:

```sh
./script/build_and_run.sh --verify
```

This installs the deployed bundle at `~/Applications/VidChopper.app` and launches it. Use
`./script/build_and_run.sh --no-launch` to stage without opening the app. For the full local workflow,
including the published DMG trust flow and optional CLI install, see [macOS 1.2.0 Installation](local-macos-install.md).

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

A successful source GUI compile/smoke lane is not a substitute for the exact packaged-candidate gate.
Linux results remain source/CI evidence only; macOS end-user support requires the published, physically
qualified 1.2.0 candidates.

## Tool and encoder checks

Run the external tools separately before a real media test:

```sh
ffmpeg -version
ffprobe -version
```

The resolver checks an explicit configured executable first, then `PATH`, common Homebrew locations such
as `/opt/homebrew/bin` and `/usr/local/bin`, and standard Unix locations such as `/usr/local/bin` and
`/usr/bin`. It normalizes duplicate candidates, verifies executability, runs `-version`, parses the
version, and blocks versions below 6.1 or at major 10 and above. The current 1.2.0 source accepts major
9.x. A supported ffmpeg/ffprobe
version mismatch remains a visible warning with both paths and versions.

Auto encoder selection must use a real minimal capability encode before hardware export. Auto chooses
usable HEVC VideoToolbox on Apple Silicon only in the `1.2.0` contract; Windows/Linux use usable HEVC
NVENC when available; all other cases resolve to x264. A failed Auto capability test records the reason
and falls back to x264 before export. An explicit hardware failure blocks export and never silently
changes the stored preference.

## Evidence boundary

Record the source commit, OS/version/architecture, compiler, Qt version, FFmpeg and ffprobe paths and
versions, lane outcome, and any failure or retest note in the [1.2.0 release evidence record](1.2.0-release-evidence.md).

Linux package installation remains intentionally absent because public Linux packages are out of scope
until 1.3.0.
