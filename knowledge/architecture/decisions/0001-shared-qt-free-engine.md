# ADR 0001: Use One Shared Qt-Free Engine

## Status

Accepted

## Context

VidChopper exposes the same video-chapter workflow through a desktop GUI and a CLI. If each entry
point owns a separate implementation of ChapterSource resolution, planning, validation, and export
orchestration, their behavior can drift even when they share lower-level helpers. The stable-release
architecture needs one behavior contract while keeping graphical dependencies out of automation and
headless execution paths.

## Alternatives Considered

- Keep independent GUI and CLI engines and synchronize behavior through duplicated tests.
- Share only low-level domain utilities while each entry point retains its own orchestration.
- Use one shared Qt-free engine and keep the GUI and CLI as adapters around it.

## Decision

Use one shared Qt-free engine for both the GUI and CLI. The shared engine owns domain workflow such
as resolving a ChapterSource, validating and planning a Job, executing planned work, and reporting
results. The GUI and CLI translate their inputs and presentation concerns at their respective
boundaries. Shared engine interfaces must not expose Qt types or require a Qt runtime.

## Consequences

- GUI and CLI behavior can be verified against the same workflow implementation.
- Headless builds and tests do not require Qt.
- UI-specific concerns remain in the GUI adapter, and CLI-specific parsing and presentation remain
  in the CLI adapter.
- Shared interfaces require explicit data and progress contracts instead of relying on framework
  objects.

## Migration Impact

Migration is incremental. Existing GUI and CLI behavior remains in place while shared workflow
services are introduced behind compatibility-preserving adapters. Tests must protect command
planning, naming, settings precedence, progress, and failure behavior while orchestration moves to
the shared engine. No public behavior changes solely because of this decision.
