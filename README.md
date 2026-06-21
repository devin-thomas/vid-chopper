# VidChopper

VidChopper is a Windows desktop application for turning one source video into chapter clips with `ffmpeg`. It is built in modern C++ on Qt 6 Widgets, with fast native execution, a dark-first interface, GPU-aware encoder selection, and a testable core that stays usable even when the full Qt SDK is not installed locally.

## Project Status

The repository is structured as a production-oriented desktop application rather than a tutorial exercise. The current codebase includes:

- A Qt 6 desktop shell for loading a video, importing embedded chapters, editing chapter timing and names, choosing output locations, and exporting clips
- A C++ core library for chapter validation, timestamp parsing and formatting, file naming, output planning, and `ffmpeg` command construction
- Automatic preference for HEVC NVENC when an NVIDIA GPU and `hevc_nvenc` support are both detected, with x264 used otherwise
- A staged test suite split into fast unit-level coverage and slower `ffmpeg` integration coverage
- A static documentation site in `docs/` intended for GitHub Pages publication

## Key Features

- Load a local video and probe it with `ffprobe`
- Start from embedded chapter metadata when present, or generate an evenly distributed starter layout with a default of six chapters
- Edit chapter names and timestamps directly in the chapter grid
- Work in either millisecond timestamps (`HH:MM:SS.mmm`) or frame timestamps (`HH:MM:SS:FF`)
- Enforce a 255 chapter maximum and a one-second minimum chapter length
- Default export folder generation beside the source file, with manual override support
- Sequential `ffmpeg` export orchestration with progress reporting and optional output verification
- Detailed advanced settings for encoding, naming, container choice, seek mode, manifest output, metadata handling, and tool paths
- Always-dark interface without exposing theme switching

## Build and Test

### Prerequisites

- Windows 10 or newer
- C++20-capable MSVC toolchain
- CMake 3.28+
- `ffmpeg` and `ffprobe` on `PATH`, or custom paths configured in the advanced settings dialog
- Qt 6.9 desktop libraries for the full GUI build

### Local Core-Only Validation

The repository intentionally separates the non-Qt core from the GUI shell. That allows fast local verification on machines that have MSVC and `ffmpeg`, but not the Qt SDK.

```powershell
cmake --preset core-release
cmake --build --preset core-release
ctest --test-dir build/core-release -C Release -L fast --output-on-failure
ctest --test-dir build/core-release -C Release -L slow --output-on-failure
```

### Full GUI Build

The GUI target is enabled when Qt 6 is available to CMake.

```powershell
cmake --preset windows-gui-release
cmake --build --preset windows-gui-release
```

## Test Strategy

The test suite is intentionally staged so the majority of checks run quickly and continuously, while a smaller number of heavier checks validate the real media pipeline.

- `fast`: pure C++ unit-level tests for timestamp parsing, chapter planning, file-name sanitization, and command generation
- `slow`: an `ffmpeg`-backed integration test that synthesizes a sample video, exports chapter clips, and verifies output durations

This split exists because the export path depends on external media tooling, while most correctness issues can be caught in pure logic tests with near-zero runtime.

## Design Notes

### Why Qt Widgets Instead of QML

VidChopper targets a utility-style desktop workflow where dense tables, settings forms, explicit menus, and direct process orchestration matter more than animated scene composition. Qt Widgets keeps that interaction model straightforward and keeps native startup and runtime costs low.

### Why the Core is Separated from the GUI

The chapter domain, timestamp handling, and command planning are not inherently graphical concerns. Keeping that logic in `src/core/` makes the code easier to reason about, easier to test, and easier to port later to other front ends or automation flows.

### Why Re-Encode by Default

Chapter boundaries can fall between keyframes. Re-encoding with x264 or HEVC NVENC makes those boundaries reliable instead of depending on keyframe-aligned stream copy behavior. The advanced settings still expose seek and audio tradeoffs for users who want different operating characteristics.

## Current Limitations

- The GUI build depends on a Qt 6 SDK; the repository can validate the core without Qt, but not the full GUI executable
- Export currently runs sequentially, which is simpler and safer for accurate progress tracking than concurrent multi-process encoding
- Frame-mode editing uses the probed video frame rate rounded to a whole-number display FPS for the table editor
- Existing embedded chapters are imported as editable start/end segments, but advanced source metadata mapping is intentionally conservative

## Repository Structure

- `src/core/`: domain logic, timestamp handling, chapter planning, naming, and `ffmpeg` command construction
- `src/qt/`: Qt application shell, settings persistence, chapter table model, ffprobe integration, GPU detection, and export coordination
- `tests/`: staged native tests
- `docs/`: GitHub Pages content
- `.github/workflows/`: CI definitions

## Licensing

This repository is released under the MIT License. Qt and FFmpeg remain separate dependencies with their own licensing terms and distribution obligations. If you distribute a built application, review the relevant Qt and FFmpeg license requirements rather than treating this repository license as the only obligation in scope.
