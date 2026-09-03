# VidChopper

VidChopper is a local desktop application for turning one source video into chapter clips with `ffmpeg`. It is built in modern C++ on Qt 6 Widgets, with fast native execution, a dark-first interface, GPU-aware encoder selection, and a testable core that stays usable even when the full Qt SDK is not installed locally.

## 1.2.0 Release Boundary

`1.2.0` is the first Apple Silicon macOS end-user release and a cumulative Windows 10/11 x64 release. It publishes a Windows ZIP, a macOS DMG, a standalone Mac CLI archive, and adjacent checksums after the exact candidates pass both physical Macs. Intel Macs are not qualified. Linux remains source- and CI-compatible without a supported end-user package until 1.3.0.

Until that promotion gate passes, `v1.1.0` remains the current public stable download. See the [1.2.0 support matrix](docs/support-matrix.md), [macOS installation guide](docs/local-macos-install.md), and [release evidence record](docs/1.2.0-release-evidence.md) for the exact boundary.

## Apple Silicon macOS Candidate

The release source builds an arm64 app and Qt-free CLI without remote media processing. From the repository root, run `./script/build_and_run.sh --verify` to build, deploy, install at `~/Applications/VidChopper.app`, and launch it. The [macOS installation guide](docs/local-macos-install.md) covers the public DMG trust flow, optional CLI install, and local packaging. `ffmpeg` and `ffprobe` remain separate local dependencies.

## Windows Download

If you want to run VidChopper on Windows 10/11 x64 without building from source, use the stable `v1.1.0` GitHub release zip:

- [Download the `v1.1.0` Windows x64 release zip](https://github.com/devin-thomas/vid-chopper/releases/download/v1.1.0/VidChopper-1.1.0-windows-x64.zip)
- [Browse all GitHub releases](https://github.com/devin-thomas/vid-chopper/releases)
- [Open the staged canonical documentation route](https://vidchopper.app/docs) (production rollout and live acceptance are tracked separately)

The release zip is a portable build that includes `VidChopper.exe`, `VidChopperCLI.exe`,
`yaml-cpp.dll`, the required Qt runtime files, and the Microsoft Visual C++ runtime. It does **not**
bundle `ffmpeg` or `ffprobe`, so those still need to be on `PATH` or configured in the advanced
settings dialog.

### Quick Start

1. Download and unzip `VidChopper-1.1.0-windows-x64.zip`.
2. Launch `VidChopper.exe`.
3. Install `ffmpeg` and `ffprobe` separately, or point VidChopper at custom tool paths in Advanced Settings.

## Project Status - 1.2.0 Release Candidate

The 1.2.0 documentation and evidence boundary covers:

- Cumulative Windows 10/11 x64 and Apple Silicon macOS candidates tied to one source commit
- A Qt 6 desktop shell for loading a video, importing embedded chapters, editing chapter timing and names, choosing output locations, and exporting clips
- A C++ core library for chapter validation, timestamp parsing and formatting, file naming, output planning, and `ffmpeg` command construction
- Cross-platform source and CI contracts for native configuration, external FFmpeg/ffprobe tools, and backend-neutral encoder resolution
- Automatic preference for HEVC NVENC on Windows/Linux and HEVC VideoToolbox on Apple Silicon only after a usable capability test, with x264 as the universal fallback
- A staged test suite split into fast unit-level coverage and slower `ffmpeg` integration coverage
- A Vite + React + TypeScript + Tailwind site in `docs/` for the canonical product, release, and CLI documentation surface, with GitHub Pages retained as a legacy mirror

The stable `v1.1.0` release remains the currently linked Windows package until the 1.2.0 physical and
promotion gates pass. It puts the GUI and CLI on one
shared cross-platform probe/export engine, packages `VidChopperCLI.exe` beside the GUI, and verifies a
ChapterBuilder-produced ChapterFile through dry-run, export, and clean release-archive smoke testing.
Linear's
[vid-chopper project](https://linear.app/devin-main/project/vid-chopper-d0e76dad962c) is the
authoritative roadmap; repository progress documents are dated snapshots.

## Key Features

- Load a local video and probe it with `ffprobe`
- Start from embedded chapter metadata when present, or generate an evenly distributed starter layout with a default of six chapters
- Edit chapter names and timestamps directly in the chapter grid
- Work in either millisecond timestamps (`HH:MM:SS.mmm`) or frame timestamps (`HH:MM:SS:FF`)
- Enforce a 255 chapter maximum and a one-second minimum chapter length
- Default export folder generation beside the source file, with manual override support
- Sequential `ffmpeg` export orchestration with progress reporting and optional output verification
- Detailed advanced settings for encoding, naming, container choice, seek mode, manifest output, metadata handling, and tool paths
- Checked settings persistence: malformed or out-of-range values fall back to documented UI defaults with a log diagnostic, and write failures remain visible instead of appearing successful
- Always-dark interface without exposing theme switching

## Contributing

Before writing code, read [`CODING_STYLE.md`](CODING_STYLE.md) — the repository's coding-style and
engineering guide. It is the source of truth for conventions (trailing return types, project type
aliases, designated initializers, aggressive `const`/`constexpr`, modern C++20 idioms, the Qt-free
core boundary, the hand-rolled test harness, and the `clang-format`/`clang-tidy` gates) and is
expected to be followed rigorously.

Use [`CONTEXT.md`](CONTEXT.md) for canonical domain vocabulary and the
[architecture decision record index](knowledge/architecture/decisions/README.md) for accepted system
boundaries and rationale. The
[shared engine and Qt boundary guide](docs/shared-engine-boundary-guide.md) is the implementation
contract for the stable `1.0.0` convergence. The preserved CLI architecture PDF is historical; its
[Markdown companion](docs/vidchopper_cli_architecture_plan.md) points to the current sources of truth.

For the broader project handoff, workflow notes, feature-progress tracking, and future-agent guidance,
start with [`knowledge/README.md`](knowledge/README.md).

## Build and Test From Source

### Source Build Prerequisites

- Windows 10/11 x64, macOS arm64, or Ubuntu x86-64 for the corresponding source/CI lane
- C++20-capable MSVC, Clang, or GCC toolchain
- CMake 3.28+
- vcpkg with the repository's pinned `nlohmann-json` and `yaml-cpp` manifest dependencies
- `ffmpeg` and `ffprobe` on `PATH`, or paths configured through the settings boundary described in [`docs/cli-settings.md`](docs/cli-settings.md)
- Qt 6.9 desktop libraries for the full GUI compile; Unix GUI smoke uses the CI/offscreen lane

Follow [`docs/build-from-source.md`](docs/build-from-source.md) for clean-checkout commands on Windows,
macOS, and Linux. A source build is not a substitute for the exact packaged-candidate and physical
acceptance gates described in the support matrix.

### Local Core-Only Validation

The repository intentionally separates the non-Qt core from the GUI shell. That allows fast local verification on machines that have MSVC and `ffmpeg`, but not the Qt SDK.

The repository-owned verification scripts are the canonical local workflow. See
[`docs/verification.md`](docs/verification.md) for tier contents, remediation, and the optional pre-push hook.
The manager-facing
[verification and release engineering guide](docs/verification-and-release-engineering.md) defines the
evidence required for review, candidate approval, publication, remote acceptance, and rollback.

```powershell
.\tools\bootstrap.ps1
.\tools\verify.ps1 -Tier Quick
.\tools\verify.ps1 -Tier Full
.\tools\verify.ps1 -Tier Release
```

Set up the pinned vcpkg toolchain once, then keep `VCPKG_ROOT` set for the preset commands:

```powershell
git clone https://github.com/microsoft/vcpkg.git .vcpkg
.\.vcpkg\bootstrap-vcpkg.bat -disableMetrics
$env:VCPKG_ROOT = (Resolve-Path .vcpkg).Path
```

On macOS or Linux, bootstrap vcpkg with `./.vcpkg/bootstrap-vcpkg.sh -disableMetrics` and export
`VCPKG_ROOT` to that checkout. The presets use this environment variable for manifest-mode dependency
resolution and never store a machine-specific path. GUI presets also require `CMAKE_PREFIX_PATH` to
point to the supplied pinned Qt 6 desktop SDK.

The CLI's Qt-free ChapterFile loader accepts `.json`, `.yaml`, and `.yml` files, applies the documented output and encoder overrides, and validates the resulting chapters before export planning.

The CLI probes inputs, plans and exports chapters, supports dry runs, reports bounded progress, and writes
per-job JSON/CSV manifests. Run `VidChopperCLI.exe --version` to confirm the installed package version.
The setup, ChapterFile, dry-run, export, manifest, error, and safety journey is staged for
[`https://vidchopper.app/docs`](https://vidchopper.app/docs). Until the production rollout is
live-validated, use the repository documentation as the authoritative source.

```powershell
cmake --preset core-release
cmake --build --preset core-release
ctest --test-dir build/core-release -C Release -L fast --output-on-failure
ctest --test-dir build/core-release -C Release -L slow --output-on-failure
```

The Windows presets retain the Visual Studio generator. `unix-core-release` is generator-neutral and
uses `build/unix-core-release`; `macos-gui-release` and `linux-gui-release` use the local default
generator and write to deterministic platform-specific build directories:

```sh
cmake --preset unix-core-release
cmake --build --preset unix-core-release
ctest --test-dir build/unix-core-release -L fast --output-on-failure

cmake --preset macos-gui-release    # macOS with CMAKE_PREFIX_PATH set
cmake --build --preset macos-gui-release
ctest --test-dir build/macos-gui-release -L qt --output-on-failure

cmake --preset linux-gui-release    # Linux with CMAKE_PREFIX_PATH set
cmake --build --preset linux-gui-release
ctest --test-dir build/linux-gui-release -L qt --output-on-failure
```

### Full GUI Build

The GUI target is enabled when Qt 6 is available to CMake.

```powershell
cmake --preset windows-gui-release
cmake --build --preset windows-gui-release
ctest --test-dir build/windows-gui-release -C Release -L qt --output-on-failure
```

### GitHub Release Packaging

The manually triggered release workflow:

- builds and smoke-tests the Windows ZIP on separate Windows 2022 runners
- imports a passed macOS Candidate run only when it has the same source commit
- aggregates and retains all six candidate/checksum files with machine-readable evidence
- promotes a named prior candidate run without rebuilding after both physical Macs pass
- pauses at the protected release environment, publishes the unchanged candidates, and compares every remote byte stream

That release asset is the intended end-user download. Building from source is only necessary for development, debugging, or local modification work.

## Test Strategy

The test suite is intentionally staged so the majority of checks run quickly and continuously, while a smaller number of heavier checks validate the real media pipeline.

- `fast`: pure C++ unit-level tests for timestamp parsing, chapter planning, file-name sanitization, and command generation
- `slow`: an `ffmpeg`-backed integration test that synthesizes a sample video, exports chapter clips, and verifies output durations
- `qt`: persisted-settings boundary tests for conversion failures, supported ranges, malformed files, and write access errors

This split exists because the export path depends on external media tooling, while most correctness issues can be caught in pure logic tests with near-zero runtime.

## CLI, Tool, and Encoder Contract

The current CLI flags are the only flags documented as released behavior: `--embedded`, `--dry-run`,
`--config`, `--config-path`, `--portable`, `--crf`, `--cq`, `--preset`, `--threads`,
`--aggregate-json`, `--aggregate-csv`, `--stop-on-first-error`, `--use-gui-config`, `--version`, and
`--help`/`-h`. The ChapterFile path is a positional argument. `--config` and `--config-path` select
one explicit CLI settings file; `--portable` selects the deterministic sidecar beside the executable.
Those modes cannot be combined. There are no `--ffmpeg` or `--ffprobe` flags; use the current
[CLI reference](docs/cli-config-schema.md) and [settings reference](docs/cli-settings.md).

VidChopper accepts external FFmpeg and ffprobe from a configured path, `PATH`, common Homebrew locations,
and standard Unix locations, then validates that each executable runs `-version` and reports a supported
version. The 1.2.0 source range is FFmpeg/ffprobe `6.1` through major `9.x`; `6.0` and `10.x` or newer are
unqualified and blocked. The published 1.1.0 foundation remains qualified through major `8.x`.
VidChopper never downloads or auto-installs these tools.

Auto, x264, and HEVC NVENC retain their existing persisted meanings. `--crf` changes x264 quality and
`--cq` changes NVENC quality only when that backend is selected; neither flag selects an encoder.
Automatic hardware selection requires a real capability test. If Auto hardware probing fails, the run
records the reason and resolves to x264 before export. An explicit hardware selection fails visibly
instead of silently changing the stored preference. HEVC VideoToolbox is supported on qualified Apple
Silicon Macs in 1.2.0.

## Design Notes

### Why Qt Widgets Instead of QML

VidChopper targets a utility-style desktop workflow where dense tables, settings forms, explicit menus, and direct process orchestration matter more than animated scene composition. Qt Widgets keeps that interaction model straightforward and keeps native startup and runtime costs low.

### Why the Core is Separated from the GUI

The chapter domain, timestamp handling, and command planning are not inherently graphical concerns. Keeping that logic in `src/core/` makes the code easier to reason about, easier to test, and easier to port later to other front ends or automation flows.

### Why Re-Encode by Default

Chapter boundaries can fall between keyframes. Re-encoding with x264 or HEVC NVENC makes those boundaries reliable instead of depending on keyframe-aligned stream copy behavior. The advanced settings still expose seek and audio tradeoffs for users who want different operating characteristics.

## Current Limitations

- The GUI build depends on a Qt 6 SDK; the repository can validate the core without Qt, but not the full GUI executable
- Intel macOS and end-user Linux packages are not supported in 1.2.0; Linux builds remain source/CI evidence only
- Export currently runs sequentially, which is simpler and safer for accurate progress tracking than concurrent multi-process encoding
- Frame-mode editing uses the probed video frame rate rounded to a whole-number display FPS for the table editor
- Existing embedded chapters are imported as editable start/end segments, but advanced source metadata mapping is intentionally conservative
- The current public package contains the GUI and CLI executables, but ffmpeg/ffprobe remain separate dependencies

## Repository Structure

- `src/core/`: domain logic, timestamp handling, chapter planning, naming, and `ffmpeg` command construction
- `src/services/`: shared Qt-free process execution and `ffprobe` metadata parsing
- `src/cli/`: Qt-free command parsing, settings, ChapterFile loading, dry-run planning, and export execution
- `src/qt/`: Qt application shell, settings persistence, chapter table model, queued probe adapter, GPU detection, and export coordination
- `tests/`: staged native tests
- `docs/`: canonical Vite + React + TypeScript + Tailwind product/docs site and legacy Pages build
- `docs/support-matrix.md`: 1.2.0 Windows/macOS support boundary and Linux non-claim
- `docs/1.2.0-release-evidence.md`: exact candidate, physical Mac, promotion, and remote verification gate
- `packaging/windows/`: bundled release notes and third-party runtime notices for the portable zip
- `.github/workflows/`: CI and release packaging definitions

## Licensing

This repository is released under the MIT License. Qt and FFmpeg remain separate dependencies with their own licensing terms and distribution obligations. If you distribute a built application, review the relevant Qt and FFmpeg license requirements rather than treating this repository license as the only obligation in scope.
