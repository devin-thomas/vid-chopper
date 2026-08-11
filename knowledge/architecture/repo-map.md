# Repo Map

## Product Shape

VidChopper is a Windows-first Qt 6 Widgets desktop application that turns one source video into a set
of chapter clips using `ffmpeg`.

Canonical domain terms are defined in [`CONTEXT.md`](../../CONTEXT.md). Accepted system boundaries
and their rationale live in the [ADR index](decisions/README.md). The
[shared engine and Qt boundary guide](../../docs/shared-engine-boundary-guide.md) is the target
dependency, service, failure, and migration contract. The preserved CLI planning PDF is a historical
snapshot; use its [Markdown companion](../../docs/vidchopper_cli_architecture_plan.md) to reach the
current sources of truth.

The repo deliberately separates the Qt-free core from the Qt UI shell so domain logic stays buildable
and testable without the full Qt SDK. The GUI and `VidChopperCLI.exe` are current entry points that
will converge on the shared engine recorded in ADR 0001.

## Repository Structure

- `src/core/`
  - Pure C++20, Qt-free domain logic
  - chapter validation
  - timecode parsing and formatting
  - output planning
  - file naming
  - command construction
- `src/services/`
  - synchronous, Qt-free workflow services shared by the GUI and CLI
  - typed process requests, results, and cancellation
  - Windows process execution behind the `ProcessExecutor` port
  - ffprobe invocation and metadata parsing
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
  - queued `ProbeCoordinator` adapter
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
- `src/services` must remain Qt-free and may depend on `src/core`.
- `src/cli` must remain Qt-free and may depend on `src/services` and `src/core`.
- `src/qt` may depend on `src/services` and `src/core`; the reverse is forbidden.
- Core types should remain standard C++ types:
  - `std::string`
  - `std::string_view`
  - `std::filesystem::path`
  - project enums and aggregates
- Qt conversions happen at the UI boundary.
- CLI settings live in `VidChopperCLI.ini`; GUI settings remain in `VidChopper.ini`.
- The CLI may read GUI settings only when the user explicitly passes `--use-gui-config`.

## Canonical Terms and Transitional Code Names

Architecture documents and issues use the glossary terms. Current implementation names with a
narrower lifecycle role do not create additional domain concepts:

| Current implementation name | Canonical interpretation |
|---|---|
| `ChapterSegment` | The current C++ aggregate representing a Chapter. |
| `BatchJob` | A source/config pairing used while resolving Jobs for a Batch. |
| `ResolvedExportJob` | A Job after settings, metadata, and planned paths have been resolved. |

These mappings let implementation migrations remain behavior-preserving. New domain documentation
should use Chapter, Job, and Batch; code names can be aligned in the implementation work that owns
their final contracts.

## Export Flow

1. The user selects a source video in the main window.
2. `ProbeCoordinator` runs the shared `ProbeService` off the UI thread to collect duration, frame rate,
   streams, and embedded chapters.
3. The UI imports embedded chapters or seeds a default layout.
4. The chapter table model exposes editable chapter rows plus the synthetic append row.
5. App settings and advanced settings determine export behavior.
6. `export_coordinator` plans through the shared service and runs `ExportEngine` on a worker thread.
7. Progress, curated logs, and raw logs are surfaced back into the main window.
8. Output lands in the default or overridden destination folder.

## CLI Flow

1. The caller runs `VidChopperCLI.exe <video> <chapters.json|chapters.yaml>` or `VidChopperCLI.exe chop <video> <config>`.
2. The CLI loads `VidChopperCLI.ini` and never reads `VidChopper.ini` unless `--use-gui-config` is passed.
3. The CLI resolves the selected ChapterSource from a ChapterFile or explicit embedded chapters.
4. The CLI probes source metadata directly through the shared Qt-free `ProbeService`.
5. The CLI validates and plans outputs through the shared Qt-free services.
6. The CLI performs dry-run output or runs the same `ExportEngine` and manifest writer as the GUI.

## Stable Contracts

Treat these as compatibility surfaces, not incidental implementation details:

- persisted enum values
- INI keys and their round-trip semantics
- output file-name patterns
- `ffmpeg` argument ordering
