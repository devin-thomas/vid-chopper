# Publishing And Workflows

The detailed local-to-release evidence contract is the
[verification and release engineering guide](../../docs/verification-and-release-engineering.md).

## Current Published State

- Branch: `main`
- Current stable tag: `v1.1.0`
- Release URL: `https://github.com/devin-thomas/vid-chopper/releases/tag/v1.1.0`
- Release asset: `https://github.com/devin-thomas/vid-chopper/releases/download/v1.1.0/VidChopper-1.1.0-windows-x64.zip`
- Checksum asset: `https://github.com/devin-thomas/vid-chopper/releases/download/v1.1.0/VidChopper-1.1.0-windows-x64.zip.sha256`
- Qualified source commit: `62ae6792d011e464f3b5553667c7933036971a4e`
- Release asset SHA-256: `00efaac3ecd8cb7486f2386e79fff67f8c7110146bbe9923e2e0f7d58d60951d`
- Checksum-file SHA-256: `bc49b8150e0bf3a19b456205f07250ad4e2b9205272a58db5ee1bc65d67642c9`
- Canonical docs URL: `https://vidchopper.app/docs`
- Clean-runner proof: the downloaded public archive started the GUI, kept the CLI Qt-free, retained
  the offline skill integrity tuple, and planned, exported, and duration-verified all 16
  ChapterBuilder chapters.

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
5. Only after the archive smoke passes, the publish job pauses at the protected `release-environment`.
6. After approval, the job creates the stable release and attaches the ZIP and SHA-256 file.
7. The publish job downloads the remote asset again and verifies its digest matches the proven candidate.

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

## Cloudflare Production Workflow

File: `.github/workflows/cloudflare.yml`

The production workflow is manual, accepts only `main`, and requires the exact
`deploy vidchopper.app` confirmation plus the `cloudflare-environment` GitHub environment. It
installs the pinned repository dependencies, validates deterministic skill artifacts, runs frontend
tests, builds and audits the canonical static artifact, performs a Wrangler dry run, publishes that
same artifact, captures the deployment identity, and runs the cache-busted remote validator.

The credential, preflight, human-gate, live-acceptance, and version-rollback procedure is maintained
in `knowledge/operations/cloudflare-production.md`. GitHub Pages remains the explicit legacy
mirror and is not a production fallback claim.

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
