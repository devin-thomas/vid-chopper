# Publishing And Workflows

## Current Published State

- Branch: `main`
- Current prerelease tag: `v0.2.0-alpha`
- Release URL: `https://github.com/devin-thomas/vid-chopper/releases/tag/v0.2.0-alpha`
- Release asset: `VidChopper-0.2.0-alpha-windows-x64.zip`

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

1. Check out the tagged source.
2. Install Qt 6.9.
3. Build the GUI preset.
4. Rebuild the core preset and run `fast` tests.
5. Package the portable Windows directory.
6. Upload `VidChopper-<version>-windows-x64.zip` as both an artifact and a release asset.

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
