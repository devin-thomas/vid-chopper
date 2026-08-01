# Publishing And Workflows

## Current Published State

- Branch: `main`
- Current prerelease tag: `v0.3.0-beta`
- Release URL: `https://github.com/devin-thomas/vid-chopper/releases/tag/v0.3.0-beta`
- Release asset: `VidChopper-0.3.0-beta-windows-x64.zip`
- Release commit: `886815578066c972de7ef400eb0b6de41a5bc33d`
- Release workflow: `https://github.com/devin-thomas/vid-chopper/actions/runs/30684000425`
- Archive SHA-256: `12c55d150f82db07b1f14545005b79edae4d0c1904eedf5107b0685dbabf70e6`
- Canonical docs URL: `https://vidchopper.app/docs` (source/build prepared by VID-51; production
  acceptance remains gated by VID-55)
- Clean-runner proof: packaged CLI planned and exported all 16 ChapterBuilder chapters, the GUI wrote
  its ready marker, the CLI ran without Qt, and the publish job matched the downloaded remote asset.

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
3. Build the Vite app in `docs/` with the GitHub Pages compatibility base and hash adapter.
4. Upload `docs/dist`.
5. Deploy through GitHub Pages.

The default docs build targets the canonical root-hosted site and validates physical HTML routes,
byte-identical machine assets, `GET`/`HEAD`, content types, cache policy, and strict machine-route
404s. The VID-55 Worker must use `html_handling = "drop-trailing-slash"` and
`not_found_handling = "404-page"`; blanket SPA fallback would turn missing machine resources into
false HTML successes. The Pages build is a separate compatibility artifact; it displays a
canonical-site notice.

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
npm ci
npm run build
npm run preview -- --host 127.0.0.1 --port 4173

# After stopping the canonical preview:
npm run build:pages
npm run preview:pages -- --host 127.0.0.1 --port 4174
```

VID-55 can reuse the same contract against a cache-busted HTTPS preview or production origin after
the deployment gate:

```powershell
node scripts/validate-routes.mjs --origin https://preview.example.com
```

Remote mode verifies the deployed origin's `GET`/`HEAD`, raw bytes, MIME/cache headers, redirects,
strict `404` behavior, immutable digests, metadata, links, and released CLI examples. It is live
acceptance evidence; the local artifact server is not.

The GUI build still depends on a locally installed Qt 6 SDK. If local Qt is absent, the remote `gui-build`
job is the source of truth for the full desktop build.
