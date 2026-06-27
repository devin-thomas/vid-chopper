# VidChopper Feature Plan — towards v0.2.0-alpha

Living tracker for the v0.2.0-alpha feature round. **Update the status of each task as it
completes** so work can be resumed easily. All code follows [`CODING_STYLE.md`](CODING_STYLE.md).

Status legend: `TODO` · `IN PROGRESS` · `DONE (PR #n merged)`

Delivery: one PR per task, smallest-first (no interdependence assumed), config before scaling so
zoom persists into the INI. Release is penultimate; the Pages rewrite is last.

---

## Task 0 — enum→bool underlying type — `DONE (PR #14 merged)`
Branch: `devin/enum-bool-underlying`
- Any `enum class` with exactly **two** enumerators that currently has underlying type `u8` is
  switched to underlying type `bool`. Values stay `0`/`1`, so `QSettings`/INI persistence is
  byte-identical and `clamp_to_enum` still works (`static_cast<int>`/`static_cast<E>` over `{0,1}`).
- Affected: `TimestampDisplayMode`, `AudioMode`, `SeekMode`.
- Unaffected (3 options): `EncoderKind`, `ContainerMode`, `OverwriteMode`.
- Add `static_assert(std::is_same_v<std::underlying_type_t<E>, bool>)` locks; document the
  two-state-enum rule in `CODING_STYLE.md` §12.

## Task 1 — Export button styling — `IN PROGRESS`
- `Export Chapters` = bold, prominent **blue** (white text, larger); flips to **red**
  `Cancel Export` while exporting. Styled for the dark theme.

## Task 2 — Chapter table columns — `TODO`
- All 4 columns stretch to fill width evenly; per-column minimum = full header-label width
  (so `Start (HH:MM:SS.mmm)` never truncates). Horizontal scrollbar disabled; columns cannot run
  off-screen. Even stretch (not content-proportional).

## Task 3 — Path slash cleanup — `TODO`
- Display paths with native separators (`QDir::toNativeSeparators`); store canonical
  `std::filesystem::path`; ffmpeg-arg paths stay OS-native. Source/output line edits + log lines.
- Keep core platform-agnostic so Linux/macOS-native paths remain possible later.

## Task 4 — Confirmation dialogs — `TODO`
- Confirm on **Remove Selected** (any count) and on **both** exit paths (File > Exit + window X).
- Add Advanced-settings toggles to disable each confirmation, wrapped in a warning.

## Task 5 — Chapter-list UX — `TODO`
- Move the **Add Chapter** button to directly above the table.
- Add a synthetic non-editable final row `➕ New Chapter…` that appends a chapter using the same
  split-last-in-half logic as the button.

## Task 6 — Explicit INI config file — `TODO`
- Switch `QSettings` to an **INI file** `VidChopper.ini` **next to the executable**, created on
  first launch if absent; fall back to `%APPDATA%/VidChopper/VidChopper.ini` if the exe dir is not
  writable. Log where the config was loaded from. No registry migration.

## Task 7 — Logging overhaul — `TODO`
- Replace the always-on log box with a collapsible **`▸ Show Logs`** disclosure, collapsed by
  default. When expanded, an **Advanced** checkbox:
  - unchecked (default): only curated, user-friendly messages via a seeded translation dictionary.
  - checked: full raw log (today's behavior).
- Categories are an `enum class LogCategory { … }`; default view = allowlist + translations.

## Task 8 — UI scaling (View > Zoom) — `TODO`
- App-wide UI scale factor (base font + key metrics/icon sizes via stylesheet).
- Range 50%–300%, **25%** steps; `Ctrl+=` / `Ctrl+-` and `Ctrl+mouse-wheel`; **no** `Ctrl+0`.
- New **View** menu **before Advanced**: Zoom In / Zoom Out / Reset (+ presets). Persist zoom.
- Auto-default: 100% at 1080px tall, linear with primary screen logical height (2160→200%),
  snapped to the nearest step; store the screen resolution in config and **re-apply auto** when the
  current resolution differs from the stored one, otherwise respect the saved zoom.

## Task 9 — Release v0.2.0-alpha — `TODO` (penultimate)
- After tasks 0–8 merge: cut prerelease `v0.2.0-alpha`; bump version references.

## Task 10 — GitHub Pages → React/TypeScript — `TODO` (last)
- Rewrite `docs/` as Vite + React + TypeScript + Tailwind; deploy via a GitHub Actions Pages
  workflow. Match the clapperboard icon branding + dark theme; use Thumio's fonts and clean design
  principles. Update to the new version + download links.
