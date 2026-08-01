# Architecture Decision Records

VidChopper architecture decision records (ADRs) preserve the rationale for choices that are costly
to reverse. Domain terms in every ADR use the canonical definitions in
[`CONTEXT.md`](../../../CONTEXT.md).

## Convention

- ADR filenames use a stable four-digit sequence and a short decision slug.
- Accepted ADRs are not rewritten to hide a later change; a new ADR supersedes the old decision.
- Every ADR states its status, context, alternatives considered, decision, consequences, and
  migration impact.
- Product roadmap state and implementation acceptance remain authoritative in Linear.

## Accepted Decisions

| ADR | Decision |
|---|---|
| [0001](0001-shared-qt-free-engine.md) | One shared Qt-free engine serves the GUI and CLI. |
| [0002](0002-pinned-vcpkg-dependencies.md) | nlohmann-json and yaml-cpp are pinned through the vcpkg manifest. |
| [0003](0003-windows-support-portability.md) | Windows 10/11 x64 is the beta support promise while interfaces remain portable. |

## Historical Material

The preserved CLI planning PDF predates these decisions. Its
[Markdown companion](../../../docs/vidchopper_cli_architecture_plan.md) identifies the current sources
of truth without changing the historical artifact.
