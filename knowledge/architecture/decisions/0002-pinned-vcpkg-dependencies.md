# ADR 0002: Pin JSON and YAML Dependencies with vcpkg

## Status

Accepted

## Context

ChapterFiles use JSON or YAML. Building that contract consistently across developer machines,
continuous integration, and Windows release packaging requires deterministic dependency versions
and one auditable dependency declaration.

## Alternatives Considered

- Depend on whichever system packages are installed on each machine.
- Download libraries directly during the CMake configure step.
- Use a vcpkg manifest without a fixed baseline.
- Pin nlohmann-json and yaml-cpp through the repository vcpkg manifest and baseline.

## Decision

Declare nlohmann-json and yaml-cpp in the repository `vcpkg.json` manifest and pin their resolution
with the manifest's `builtin-baseline`. Dependency upgrades are deliberate repository changes that
must pass the same build, test, packaging, and licensing checks as other release changes.

## Consequences

- Local, CI, and release builds resolve the same dependency baseline.
- Dependency changes are visible and reviewable in version control.
- Contributors need a vcpkg checkout compatible with the pinned baseline.
- Security or compatibility upgrades require an explicit baseline update and verification pass.

## Migration Impact

The current manifest already declares both dependencies and a fixed baseline, so adopting this ADR
does not change runtime behavior. Future shared-engine work consumes these manifest dependencies
rather than introducing a second package source or an unpinned fallback.
