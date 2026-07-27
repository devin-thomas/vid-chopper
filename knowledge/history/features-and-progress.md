# Features And Progress

Snapshot date: 2026-07-27. Linear is authoritative for live status and acceptance criteria:
[vid-chopper project](https://linear.app/devin-main/project/vid-chopper-d0e76dad962c).

## Current Delivery State

The `v0.2.0-alpha` feature round is complete, including the Pages rewrite.

The current `main` branch includes the first no-Qt CLI phase: the `VidChopperCLI.exe` target,
command contract, separate `VidChopperCLI.ini` settings boundary, JSON/YAML schema, ChapterFile
loader, and fixture coverage. The loader is integrated on `main`; export execution is not.

The active release objective is `0.3.0-alpha`: finish and package the deterministic Qt-free CLI
alongside the existing GUI. Shared-engine GUI migration remains the later `1.0.0-beta` objective.

## Task Status

- Task 0: enum underlying-type migration for two-state enums - done
- Task 1: export button styling and cancel state - done
- Task 2: chapter table equal-width columns and no horizontal scrolling - done
- Task 3: path normalization and native separator cleanup - done
- Task 4: confirmation dialogs and advanced-settings toggles - done
- Task 5: chapter-list UX improvements and synthetic append row - done
- Task 6: explicit `VidChopper.ini` handling with fallback and logging - done
- Task 7: collapsible logging and curated/raw log split - done
- Task 8: app-wide zoom controls and persistence - done
- Task 9: release `v0.2.0-alpha` - done
- Task 10: GitHub Pages rewrite to Vite, React, TypeScript, and Tailwind - done
- VID-CLI-1: no-Qt CLI target skeleton - done on `main`
- VID-CLI-2: CLI command contract and invocation model - done on `main`
- VID-CLI-3: separate `VidChopperCLI.ini` settings boundary - done on `main`
- VID-CLI-4: JSON/YAML chapter configuration schema - done on `main`
- VID-CLI-5 (`VID-16`): JSON/YAML ChapterFile loader integration - done on `main`

## Current Linear Frontier

- `VID-17`: explicit chapter-source policy - next implementation issue
- `VID-18` through `VID-26`: probing, export, batching, planning, dry-run, progress, manifests,
  flags, tests, packaging, and docs - backlog in roadmap order
- `VID-31` through `VID-35`: verification parity and release-critical correctness gates - backlog
- `VID-47`: ChapterBuilder-to-VidChopper end-to-end release validation - release-blocking backlog
- `VID-36`: package, publish, and verify `0.3.0-alpha` - final release task

## Release Milestones

- Desktop feature round landed on `main`.
- Pages deployment workflow landed on `main`.
- `v0.2.0-alpha` was published as a GitHub prerelease.
- The release workflow produced and attached `VidChopper-0.2.0-alpha-windows-x64.zip`.
- The Pages rewrite now publishes a multi-page site that covers the product landing page, release portal, and developer docs surface.

## Reconciliation Note

The VID-29 reconciliation merged the fetched `origin/main` CLI commits into the preserved demo,
Pages, release-metadata, and Qt work. Linear issues `VID-12` through `VID-16` have matching
implementation on `main`. The 2026-07-27 tracking reconciliation also corrected shipped
`v0.2.0-alpha` issues `VID-1` through `VID-9` to Done and added the ChapterBuilder compatibility gate.

## Pages Rewrite Direction That Shipped

- Multi-page site, not a single landing page
- Three surface roles:
  - product landing page
  - release portal
  - developer docs page
- Visual direction:
  - built around the VidChopper icon palette
  - black-to-violet cinematic desktop-tool styling
  - clean, restrained execution rather than generic SaaS chrome
- Homepage priorities:
  - release ZIP download first
  - screenshots and workflow previews second
  - no contribution-focused primary copy
