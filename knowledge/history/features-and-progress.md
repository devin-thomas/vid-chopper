# Features And Progress

## Current Delivery State

The `v0.2.0-alpha` feature round is complete, including the Pages rewrite.

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

## Release Milestones

- Desktop feature round landed on `main`.
- Pages deployment workflow landed on `main`.
- `v0.2.0-alpha` was published as a GitHub prerelease.
- The release workflow produced and attached `VidChopper-windows-x64.zip`.
- The Pages rewrite now publishes a multi-page site that covers the product landing page, release portal, and developer docs surface.

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
