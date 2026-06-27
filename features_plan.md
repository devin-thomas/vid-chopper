# VidChopper Feature Plan — towards v0.2.0-alpha

Living tracker for the v0.2.0-alpha feature round. Update the status of each task as it
completes so work can be resumed easily. The implementation in this checkout follows the
repository coding style documented on the `devin/enum-bool-underlying` branch.

Status legend: `TODO` · `IN PROGRESS` · `DONE`

Linear tracking:
- `VID-1` Task 0
- `VID-2` Task 1
- `VID-3` Task 2
- `VID-4` Task 3
- `VID-5` Task 4
- `VID-6` Task 5
- `VID-7` Task 6
- `VID-8` Task 7
- `VID-9` Task 8

Delivery: one PR per task, smallest-first. Config lands before scaling so zoom persists into
the INI. Release is penultimate; the Pages rewrite is last.

---

## Task 0 — enum→bool underlying type — `DONE`
Branch: `devin/enum-bool-underlying`
- Any `enum class` with exactly two enumerators that currently has underlying type `u8` is
  switched to underlying type `bool`.
- Values stay `0` and `1`, so `QSettings` and INI persistence is byte-identical and enum
  clamping over `{0,1}` still works.
- Affected: `TimestampDisplayMode`, `AudioMode`, `SeekMode`.
- Unaffected: `EncoderKind`, `ContainerMode`, `OverwriteMode`.

## Task 1 — Export button styling — `DONE`
- `Export Chapters` is a bold, prominent blue button in the dark theme.
- While exporting, it flips to a red `Cancel Export` state.

## Task 2 — Chapter table columns — `DONE`
- All 4 columns stretch to fill width evenly.
- The table uses a minimum section width based on the widest header label.
- Horizontal scrolling is disabled.

## Task 3 — Path slash cleanup — `DONE`
- Display paths with native separators.
- Store canonical `std::filesystem::path` values at the Qt boundary.
- Keep ffmpeg-facing paths OS-native.

## Task 4 — Confirmation dialogs — `DONE`
- Confirm on `Remove Selected`.
- Confirm on both exit paths.
- Add advanced-settings toggles to disable each confirmation with warning copy.

## Task 5 — Chapter-list UX — `DONE`
- Move the `Add Chapter` button directly above the table.
- Add a synthetic non-editable final row `➕ New Chapter…` that appends a chapter using the
  same split-last-in-half logic as the button.

## Task 6 — Explicit INI config file — `DONE`
- Switch `QSettings` to `VidChopper.ini` next to the executable.
- Fall back to `%APPDATA%/VidChopper/VidChopper.ini` if the executable directory is not writable.
- Log where the config was loaded from.

## Task 7 — Logging overhaul — `DONE`
- Replace the always-on log box with a collapsible `▸ Show Logs` disclosure, collapsed by default.
- When expanded, the `Advanced` checkbox switches between curated and raw logs.
- Categories use `enum class LogCategory`.

## Task 8 — UI scaling (View > Zoom) — `DONE`
- App-wide UI scale factor with 50%–300% zoom in 25% steps.
- Support `Ctrl+=`, `Ctrl+-`, and `Ctrl+mouse-wheel`.
- Add a `View` menu before `Advanced`.
- Persist zoom and re-apply the auto zoom when the screen resolution changes.

## Task 9 — Release v0.2.0-alpha — `DONE`
- After tasks 0–8 merge: cut prerelease `v0.2.0-alpha`; bump version references.

## Task 10 — GitHub Pages → React/TypeScript — `DONE`
- Rewrite `docs/` as Vite + React + TypeScript + Tailwind.
- Deploy via a GitHub Actions Pages workflow and update the current download/version content.
- Pages rewrite brief captured and shipped:
  - multi-page site spanning product landing, release portal, and developer docs
  - visual direction centered on the app icon, using Thumio as the style bible
  - homepage priorities: release ZIP download first, screenshots second, no direct contribution CTA
