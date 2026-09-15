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
- Release target commit: `ceb104f9922efd8909a875ae0861d8e878a949fd`
- Published at: `2026-08-11T23:04:40Z`
- Release asset size: `49,467,494` bytes
- Release asset SHA-256: `00efaac3ecd8cb7486f2386e79fff67f8c7110146bbe9923e2e0f7d58d60951d`
- Checksum-file SHA-256: `bc49b8150e0bf3a19b456205f07250ad4e2b9205272a58db5ee1bc65d67642c9`
- Canonical docs URL: `https://vidchopper.app/docs`
- Clean-runner proof: the downloaded public archive started the GUI, kept the CLI Qt-free, retained
  the offline skill integrity tuple, and planned, exported, and duration-verified all 16
  ChapterBuilder chapters.
- Public verification: the downloaded GitHub ZIP matches the Titan candidate byte-for-byte, and the
  Windows archive verifier passes against the public asset with Qt 6.9.3 and FFmpeg/ffprobe 8.1.2.

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

## Cloudflare Pages Workflow

File: `.github/workflows/cloudflare.yml`

The docs build targets the root-hosted site and validates physical HTML routes, byte-identical
machine assets, `GET`/`HEAD`, content types, cache policy, and strict machine-route 404s. HTML
routes are flat `route.html` files so Cloudflare Pages serves them without a trailing slash, and the
root `404.html` keeps Pages from falling back to the SPA shell, which would turn missing machine
resources into false HTML successes.

Every push to `main` that touches site inputs publishes `vidchopper.app` automatically; it can also
be dispatched manually on `main`. Using the `cloudflare-environment` GitHub environment, it installs
the pinned dependencies, checks the Pages credentials, validates deterministic skill artifacts, runs
frontend tests, builds and audits the canonical static artifact, uploads it with
`wrangler pages deploy`, and runs the cache-busted remote validator.

Credentials, manual deploys, previews, routing, and rollback are documented in
`knowledge/operations/cloudflare-production.md`. The former GitHub Pages mirror is retired.

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

Site path:

```powershell
cd docs
npm ci
npm run build
npm run preview -- --host 127.0.0.1 --port 4173
```

The same contract checks any Cloudflare Pages preview or the production origin:

```powershell
node scripts/validate-routes.mjs --origin https://my-change.vidchopper.pages.dev
```

Remote mode verifies the deployed origin's `GET`/`HEAD`, raw bytes, MIME/cache headers, redirects,
strict `404` behavior, immutable digests, metadata, links, and released CLI examples. It is live
acceptance evidence; the local artifact server is not.

The GUI build still depends on a locally installed Qt 6 SDK. If local Qt is absent, the remote `gui-build`
job is the source of truth for the full desktop build.
