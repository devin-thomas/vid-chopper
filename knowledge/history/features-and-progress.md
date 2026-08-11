# Features And Progress

Snapshot date: 2026-08-11. Linear is authoritative for live status and acceptance criteria:
[vid-chopper project](https://linear.app/devin-main/project/vid-chopper-d0e76dad962c).

## Current Delivery State

The `v0.2.0-alpha` feature round is complete, including the Pages rewrite.

The stable `1.0.0` objective is shipped. The desktop app and CLI use the same Qt-free process,
probe, planning, export, progress, verification, and manifest services. The supported Windows x64
archive is published and verified from the public download.

Update checks (`VID-10`) and broader test-architecture expansion (`VID-43`) were explicitly canceled
as 1.0 requirements. Public documentation presentation polish remains separate post-1.0 work.

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

- `VID-17` through `VID-26`: complete on `main`
- `VID-31` through `VID-35`: complete on `main`
- `VID-47`: ChapterBuilder-to-VidChopper end-to-end release validation - done
- `VID-36`: package, publish, and verify `0.3.0-beta` - done
- `VID-37` through `VID-42`, plus `VID-45`: shared-engine architecture, migration, and hardening - done
- `VID-44`: package, publish, and remotely verify stable `1.0.0` - done
- `VID-28`: shared-engine convergence roadmap for stable `1.0.0` - done

## Release Milestones

- Desktop feature round landed on `main`.
- Pages deployment workflow landed on `main`.
- `v0.2.0-alpha` was published as a GitHub prerelease.
- The release workflow produced and attached `VidChopper-0.2.0-alpha-windows-x64.zip`.
- `v0.3.0-beta` packages the complete Qt-free CLI beside the GUI and proves the archive in a second
  clean Windows runner before publication.
- The TNS 2XKO #36 ChapterBuilder fixture plans and exports all 16 chapters through the packaged CLI.
- `v1.0.0` puts the GUI and CLI on the shared engine and publishes the verified Windows x64 archive
  from commit `cccbe88b51a3766bb0dd94e1321c2a257d9e07c5`.
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
