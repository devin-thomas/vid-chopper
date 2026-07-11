# Repo Map

## Product Shape

VidChopper is a Windows-first Qt 6 Widgets desktop application that turns one source video into a set
of chapter clips using `ffmpeg`.

The repo deliberately separates the Qt-free core from the Qt UI shell so the domain logic stays buildable
and testable without the full Qt SDK. A no-Qt `VidChopperCLI.exe` target is being introduced as the
power-user and automation-facing entry point.

## Repository Structure

- `src/core/`
  - Pure C++20, Qt-free domain logic
  - chapter validation
  - timecode parsing and formatting
  - output planning
  - file naming
  - command construction
- `src/cli/`
  - no-Qt command-line entry point and CLI-specific orchestration
  - command parsing
  - CLI settings path handling
  - JSON/YAML ChapterFile loading and batch planning
- `src/qt/`
  - Qt 6 Widgets shell
  - main window
  - advanced settings dialog
  - chapter table model
  - app settings persistence
  - `ffprobe` probing
  - GPU detection
  - export coordination
- `tests/`
  - hand-rolled test harness
  - `fast` pure-logic coverage
  - CLI parser and settings boundary coverage
  - `tests/dummy/` mock fixtures for CLI config, settings, path, and future batch/probing tests
  - `slow` ffmpeg-backed integration coverage
- `docs/`
  - Vite + React + TypeScript + Tailwind Pages app
  - public-facing product, release, and developer-doc site
- `knowledge/`
  - future-agent handoff docs and structured project memory
- `packaging/windows/`
  - release-bundle readme and third-party notices
- `.github/workflows/`
  - CI, release packaging, and Pages deployment definitions

## Architecture Boundaries

- `src/core` must remain Qt-free.
- `src/cli` must remain Qt-free and may depend on `src/core`.
- `src/qt` may depend on `src/core`; the reverse is forbidden.
- Core types should remain standard C++ types:
  - `std::string`
  - `std::string_view`
  - `std::filesystem::path`
  - project enums and aggregates
- Qt conversions happen at the UI boundary.
- CLI settings live in `VidChopperCLI.ini`; GUI settings remain in `VidChopper.ini`.
- The CLI may read GUI settings only when the user explicitly passes `--use-gui-config`.

## Export Flow

1. The user selects a source video in the main window.
2. `ffprobe_service` probes the file for duration, frame rate, and embedded chapters.
3. The UI imports embedded chapters or seeds a default layout.
4. The chapter table model exposes editable chapter rows plus the synthetic append row.
5. App settings and advanced settings determine export behavior.
6. `export_coordinator` validates the chapter plan and runs sequential `ffmpeg` jobs.
7. Progress, curated logs, and raw logs are surfaced back into the main window.
8. Output lands in the default or overridden destination folder.

## CLI Flow Target

1. The caller runs `VidChopperCLI.exe <video> <chapters.json|chapters.yaml>` or `VidChopperCLI.exe chop <video> <config>`.
2. The CLI loads `VidChopperCLI.ini` and never reads `VidChopper.ini` unless `--use-gui-config` is passed.
3. The CLI resolves explicit chapter configuration from JSON/YAML.
4. The CLI probes source metadata through a no-Qt probing service.
5. The CLI validates and plans outputs through `src/core`.
6. The CLI performs dry-run output or sequential export with human-readable progress.

## Stable Contracts

Treat these as compatibility surfaces, not incidental implementation details:

- persisted enum values
- INI keys and their round-trip semantics
- output file-name patterns
- `ffmpeg` argument ordering
