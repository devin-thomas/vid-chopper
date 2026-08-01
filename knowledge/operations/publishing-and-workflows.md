# Publishing And Workflows

## Current Published State

- Branch: `main`
- Current prerelease tag: `v0.3.0-beta`
- Release URL: `https://github.com/devin-thomas/vid-chopper/releases/tag/v0.3.0-beta`
- Release asset: `VidChopper-0.3.0-beta-windows-x64.zip`

## CI Workflow

File: `.github/workflows/ci.yml`

Jobs:

- `lint`
  - Ubuntu
  - pinned `clang-format` and `clang-tidy`
  - formatting check over `src` and `tests`
  - core-only `clang-tidy`
- `core-tests`
  - Windows
  - installs `ffmpeg`
  - configures/builds `core-release`
  - runs `fast` and `slow` tests
- `gui-build`
  - Windows
  - installs Qt 6.9
  - configures/builds `windows-gui-release`

## Release Workflow

File: `.github/workflows/release.yml`

Behavior:

1. A maintainer manually supplies the release version and whether to publish.
2. A Windows 2022 runner builds the GUI and CLI, runs native tests, and packages the portable ZIP.
3. A second fresh Windows 2022 runner downloads and extracts that exact candidate artifact.
4. The clean runner verifies the packaged GUI, isolated Qt-free CLI, version/help/direct/chop modes,
   the 16-chapter ChapterBuilder fixture, actual exports, and manifests.
5. Only after the archive smoke passes, the publish job creates the prerelease and attaches the ZIP
   and SHA-256 file.
6. The publish job downloads the remote asset again and verifies its digest matches the proven candidate.

## Pages Workflow

File: `.github/workflows/pages.yml`

Behavior:

1. Check out the repo.
2. Install Node dependencies from `docs/`.
3. Build the Vite app in `docs/`.
4. Upload `docs/dist`.
5. Deploy through GitHub Pages.

## Local Validation Reality

Reliable local path:

```powershell
cmake --preset core-release
cmake --build --preset core-release
ctest --test-dir build/core-release -C Release -L fast --output-on-failure
```

GUI path:

```powershell
cmake --preset windows-gui-release
cmake --build --preset windows-gui-release
```

Pages path:

```powershell
cd docs
npm install
npm run build
npm run preview
```

The GUI build still depends on a locally installed Qt 6 SDK. If local Qt is absent, the remote `gui-build`
job is the source of truth for the full desktop build.
