# VidChopper Knowledge Base

This directory is the durable repo handoff for future agents and human contributors working in
`devin-thomas/vid-chopper`.

It is intentionally split by topic so a future session can read the relevant slices directly instead of
reconstructing the codebase from scratch.

## Current State

- Primary branch: `main`
- Latest published prerelease: `v0.3.0-beta`
- Public release asset: `VidChopper-0.3.0-beta-windows-x64.zip`
- Completed beta roadmap: Linear [`VID-27`](https://linear.app/devin-main/issue/VID-27), containing
  the complete Qt-free CLI and release safety gates
- Completed compatibility gate: [`VID-47`](https://linear.app/devin-main/issue/VID-47), proving a
  ChapterBuilder-produced file through dry-run, export, and packaged CLI verification
- Current roadmap: Linear [`VID-28`](https://linear.app/devin-main/issue/VID-28) for the shared-engine
  stable `1.0.0` convergence
- Last reconciled with Linear and `main`: 2026-07-31

## Read First

- [`CONTEXT.md`](../CONTEXT.md)
- [`knowledge/architecture/decisions/README.md`](architecture/decisions/README.md)
- [`docs/shared-engine-boundary-guide.md`](../docs/shared-engine-boundary-guide.md)
- [`knowledge/architecture/repo-map.md`](architecture/repo-map.md)
- [`docs/vidchopper_cli_architecture_plan.md`](../docs/vidchopper_cli_architecture_plan.md)
- [`knowledge/coding-style/overview.md`](coding-style/overview.md)
- [`knowledge/coding-style/core-and-qt-boundary.md`](coding-style/core-and-qt-boundary.md)
- [`knowledge/coding-style/testing-and-quality-gates.md`](coding-style/testing-and-quality-gates.md)
- [`knowledge/history/features-and-progress.md`](history/features-and-progress.md)
- [`knowledge/operations/publishing-and-workflows.md`](operations/publishing-and-workflows.md)
- [`knowledge/operations/agent-workflow.md`](operations/agent-workflow.md)
- [`knowledge/operations/pages-rewrite-brief.md`](operations/pages-rewrite-brief.md)
- [`knowledge/skills/installed-skills-and-plugins.md`](skills/installed-skills-and-plugins.md)
- [`knowledge/skills/vidchopper-cli-agent-skill-design.md`](skills/vidchopper-cli-agent-skill-design.md)

## Directory Map

- `architecture/`
  - Canonical architecture decisions, repo structure, boundary rules, and the desktop export flow.
- `coding-style/`
  - Split view of the root `CODING_STYLE.md` source of truth.
- `history/`
  - Feature-plan status and milestone notes.
- `operations/`
  - Session workflow, publishing rules, release/Pages behavior, and the Pages brief.
- `skills/`
  - Installed-skill inventory, plugin notes, skill-install recovery guidance, and the VID-48
    VidChopper CLI agent-skill design and downstream contract.

## Maintenance Rules

- Treat Linear as the authoritative source for roadmap state, issue status, ordering, and acceptance
  criteria. Repository documents are dated snapshots and must not create a competing task system.
- Keep domain definitions in root `CONTEXT.md`; architecture documents should link to it instead of
  creating local synonyms or duplicate definitions.
- Update `knowledge/history/features-and-progress.md` when a release frontier or published milestone
  changes.
- Update `knowledge/operations/publishing-and-workflows.md` whenever CI, release, or Pages behavior changes.
- Keep `knowledge/operations/pages-rewrite-brief.md` in sync with future site-direction changes.
- Keep `CODING_STYLE.md` as the repo source of truth; keep the split files here aligned with it.
