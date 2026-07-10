# VidChopper Knowledge Base

This directory is the durable repo handoff for future agents and human contributors working in
`devin-thomas/vid-chopper`.

It is intentionally split by topic so a future session can read the relevant slices directly instead of
reconstructing the codebase from scratch.

## Current State

- Primary branch: `main`
- Current prerelease: `v0.2.0-alpha`
- Public release asset: `VidChopper-0.2.0-alpha-windows-x64.zip`
- Pages state:
  - Task 10 is now implemented as a Vite + React + TypeScript + Tailwind app under `docs/`
- Feature-plan state:
  - Tasks 0-10 are complete at the time of this update

## Read First

- [`knowledge/architecture/repo-map.md`](architecture/repo-map.md)
- [`knowledge/coding-style/overview.md`](coding-style/overview.md)
- [`knowledge/coding-style/core-and-qt-boundary.md`](coding-style/core-and-qt-boundary.md)
- [`knowledge/coding-style/testing-and-quality-gates.md`](coding-style/testing-and-quality-gates.md)
- [`knowledge/history/features-and-progress.md`](history/features-and-progress.md)
- [`knowledge/operations/publishing-and-workflows.md`](operations/publishing-and-workflows.md)
- [`knowledge/operations/agent-workflow.md`](operations/agent-workflow.md)
- [`knowledge/operations/pages-rewrite-brief.md`](operations/pages-rewrite-brief.md)
- [`knowledge/skills/installed-skills-and-plugins.md`](skills/installed-skills-and-plugins.md)

## Directory Map

- `architecture/`
  - Repo structure, boundary rules, and the desktop export flow.
- `coding-style/`
  - Split view of the root `CODING_STYLE.md` source of truth.
- `history/`
  - Feature-plan status and milestone notes.
- `operations/`
  - Session workflow, publishing rules, release/Pages behavior, and the Pages brief.
- `skills/`
  - Installed-skill inventory, plugin notes, and skill-install recovery guidance.

## Maintenance Rules

- Update `knowledge/history/features-and-progress.md` whenever feature-plan status changes.
- Update `knowledge/operations/publishing-and-workflows.md` whenever CI, release, or Pages behavior changes.
- Keep `knowledge/operations/pages-rewrite-brief.md` in sync with future site-direction changes.
- Keep `CODING_STYLE.md` as the repo source of truth; keep the split files here aligned with it.
