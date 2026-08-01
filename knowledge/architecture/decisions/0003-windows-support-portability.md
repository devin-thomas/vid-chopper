# ADR 0003: Support Windows Beta While Keeping Interfaces Portable

## Status

Accepted

## Context

VidChopper needs a support promise that matches the platforms actually packaged and tested during
the beta. At the same time, the shared engine should not make future platform work unnecessarily
expensive by exposing Windows-only or GUI-framework types at its boundaries.

## Alternatives Considered

- Promise support for every desktop platform before equivalent packaging and test coverage exist.
- Treat Windows-specific behavior as acceptable throughout the shared engine.
- Promise Windows 10/11 x64 beta support while keeping shared interfaces portable.

## Decision

Windows 10 and Windows 11 on x64 are the supported beta environment. Other operating systems are
not supported until their packaging, tests, and operational behavior are accepted separately.
Shared engine interfaces use portable domain values and isolate platform-specific process, path,
and packaging behavior behind boundaries.

## Consequences

- Release qualification and user support have a clear Windows 10/11 x64 target.
- The project does not imply unverified support for other operating systems.
- Portable interfaces preserve a practical path to future platform adapters.
- Windows-specific behavior must be kept out of shared contracts and tested at its boundary.

## Migration Impact

Current Windows packaging and beta behavior remain unchanged. New shared-engine interfaces avoid
Windows-only and Qt-specific values. Existing platform-specific operations can move behind adapters
incrementally, with behavior-preserving tests required before replacement.
