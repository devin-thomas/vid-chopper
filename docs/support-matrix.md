# 1.1.0 Support Matrix

`1.1.0` is a shared Unix foundation release, not a Unix binary release. A source build or a CI GUI
smoke proves that the code compiles and starts in a qualification environment; it does not make that
platform an end-user supported platform.

## End-user boundary

| Platform and architecture | 1.1.0 source/CI evidence | Public binary in 1.1.0 | End-user support in 1.1.0 | First planned support release |
| --- | --- | --- | --- | --- |
| Windows 10/11 x64 | Core, CLI, GUI, fast/slow/Qt tests, and Windows package regression | Yes: Windows portable ZIP and adjacent checksum | Yes | 1.1.0 |
| macOS 15 arm64 | Native core/CLI tests and GUI compile/launch smoke | No | No | 1.2.0 |
| macOS 26 arm64 | Native core/CLI tests and GUI compile/launch smoke when a hosted runner is available, or an equivalent documented runner | No | No | 1.2.0 |
| Ubuntu 24.04 x86-64 | Native core/CLI tests and GUI compile/launch smoke | No | No | 1.3.0 |
| Ubuntu 26.04 x86-64 | Native core/CLI tests and GUI compile/launch smoke | No | No | 1.3.0 |

Unix CI may retain internal build artifacts for debugging and evidence. Those artifacts are not release
assets, installable packages, or a support claim. `1.1.0` publishes only the passed Windows candidate;
the release must not contain a macOS or Linux binary package.

## Qualification lanes

The required `1.1.0` evidence lanes are:

- Windows x64: core, CLI, GUI, fast/slow/Qt tests, packaging regression, exact candidate smoke, and
  checksum verification.
- macOS 15 arm64: core/CLI tests plus GUI compile/launch smoke.
- macOS 26 arm64: the same gate when hosted capacity exists; otherwise record the approved equivalent
  runner and its outcome.
- Ubuntu 24.04 x86-64: core/CLI tests plus GUI compile/launch smoke.
- Ubuntu 26.04 x86-64: core/CLI tests plus GUI compile/launch smoke.

Every assigned failure blocks publication. The evidence record must bind each outcome to one source
commit, workflow/run identifier, dependency versions, and exact artifact identity where an artifact
exists.

## Build and tooling contract

Use the [clean-checkout source-build guide](build-from-source.md). All platforms use C++20, CMake 3.28+
and the pinned vcpkg manifest. FFmpeg and ffprobe remain external and are supported from 6.1 through
major 8.x after executable and `-version` validation. No platform auto-installs or downloads FFmpeg.

The CLI and GUI keep separate settings files. Native roots, explicit GUI import, ChapterFile precedence,
and the current flags are documented in [CLI settings](cli-settings.md) and the [ChapterFile schema](cli-config-schema.md).
Hardware encoders require a real capability test; Auto falls back to x264 before export when that test
fails, while an explicit hardware selection fails visibly.

## Deliberate non-claims

This matrix does not provide macOS installation instructions or Linux package installation instructions.
It does not rename the current release metadata, change the release workflow, or create a Unix package.
Those changes belong to the release/package work owned by VCU-111 and later platform-release tickets.
