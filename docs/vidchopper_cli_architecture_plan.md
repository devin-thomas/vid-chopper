# VidChopper CLI Architecture Plan (Historical)

The binary [CLI architecture plan PDF](vidchopper_cli_architecture_plan.pdf) is a preserved planning
snapshot. It predates the stable-release domain glossary and accepted architecture decisions, has no
committed editable source, and is not the current authority for terminology or architecture.

## Current Sources of Truth

- [`CONTEXT.md`](../CONTEXT.md) defines Source, Chapter, RenderedSegment, ChapterFile, ChapterSource,
  Job, and Batch.
- The [ADR index](../knowledge/architecture/decisions/README.md) records accepted system boundaries,
  dependency pinning, and the beta support promise.
- The [repository map](../knowledge/architecture/repo-map.md) describes the current code layout and
  maps transitional implementation names to the canonical domain vocabulary.
- The [ChapterFile schema reference](cli-config-schema.md) defines the current serialization and
  validation contract.

The PDF remains unchanged for provenance. New architecture material should link to the glossary and
ADRs instead of copying definitions from the historical plan.
