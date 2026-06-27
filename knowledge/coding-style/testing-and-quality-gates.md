# Testing And Quality Gates

## Test Model

VidChopper uses a hand-rolled native test harness, not GoogleTest or Catch2.

- `fast` tests are pure logic and should be the default local confidence path.
- `slow` tests validate the real `ffmpeg` export pipeline.

## Golden-Test Expectations

Add or extend golden coverage before behavior-preserving refactors that touch:

- timecode formatting or parsing
- output file naming
- INI round-trips
- `ffmpeg` command generation
- enum persistence and clamping behavior

## Lint And Formatting

Pinned tooling matters:

- `clang-format == 18.1.8`
- `clang-tidy == 18.1.8`

Formatting is enforced over `src/` and `tests/`. Core linting is enforced over `src/core`.

## CI Jobs

- `lint`
  - pinned formatter and tidy
  - formatting check over `src` and `tests`
  - core-only `clang-tidy`
- `core-tests`
  - configures/builds `core-release`
  - runs `fast` and `slow` tests on Windows
- `gui-build`
  - installs Qt 6.9
  - configures/builds `windows-gui-release`
- `pages`
  - installs the `docs/` frontend dependencies
  - builds the public Pages site
  - deploys `docs/dist`

## Stable Contracts

Treat these as compatibility surfaces:

- enum values
- INI keys and their serialized semantics
- output naming patterns
- `ffmpeg` argument ordering
