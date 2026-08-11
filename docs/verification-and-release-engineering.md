# VidChopper Verification and Release Engineering Guide

**Audience:** maintainers, reviewers, release managers, and coding agents

**Authority:** repository commands and workflows on the release commit

**Supported release host:** Windows 10/11 x64

This guide defines how VidChopper changes move from a local checkout to a verified release. Run the
repository-owned commands first, diagnose the first failing stage, and preserve evidence that ties every
claim to one commit and one candidate artifact. GitHub Actions confirms a clean runner; it does not
replace local diagnosis.

## 1.1.0 Support Boundary

`1.1.0` publishes Windows 10/11 x64 binaries only. macOS and Linux source builds, native core/CLI tests,
and Unix GUI compile/launch smoke are foundation evidence; they are not end-user support and do not
produce public Unix packages. The first planned end-user macOS release is `1.2.0`, and the first planned
end-user Linux release is `1.3.0`.

The required foundation lanes are Windows x64, macOS 15 arm64, macOS 26 arm64 when hosted or its
documented equivalent, Ubuntu 24.04 x86-64, and Ubuntu 26.04 x86-64. A failure in an assigned lane blocks
publication. Use the [support matrix](support-matrix.md) for the platform table and the
[1.1.0 evidence record](1.1.0-foundation-evidence.md) for candidate identity and lane fields.

The release/package implementation remains deliberately separate from this documentation change. This
branch does not modify release workflows, CMake/vcpkg version metadata, release manifests, or generated
package artifacts; VCU-111 owns the Windows candidate and publication path.

Canonical domain terms come from [`CONTEXT.md`](../CONTEXT.md). Accepted architecture boundaries are
recorded in the [ADR index](../knowledge/architecture/decisions/README.md).

## Manager Decision Frame

| Decision | Required evidence | Stop condition |
|---|---|---|
| Is a change ready for review? | Relevant local tier or CI lane, focused tests, and a clean diff | Any unexplained failure or missing prerequisite |
| Is a PR ready to merge? | Required PR jobs green for the reviewed head commit | Superseded, canceled, skipped-required, or red jobs |
| Is a package a release candidate? | Release tier, candidate ZIP digest, and clean-runner archive smoke | Candidate bytes differ between stages |
| Is publication authorized? | Human approval, final commit, release notes, and rollback plan | Missing approval or mutable/unknown artifact identity |
| Is a release complete? | Tag, commit, release URL, asset URL, remote digest, and supported Windows smoke | Remote asset or metadata does not match the proven candidate |

## Bootstrap Policy

VidChopper uses a hybrid bootstrap: the repository installs small pinned quality tools and a pinned local
vcpkg checkout, while large platform SDKs remain explicit host prerequisites.

```powershell
pwsh -NoProfile -File tools/bootstrap.ps1
pwsh -NoProfile -File tools/bootstrap.ps1 -CheckOnly
```

The first command creates `.venv-tools`, installs the versions in
`tools/verification-requirements.txt`, pins `.vcpkg` to `vcpkg.json`'s `builtin-baseline`, and installs
manifest dependencies. It does not install large SDKs. `-CheckOnly` is read-only and fails when a required
tool or version is missing.

| Tool or contract | Required policy | Source of truth |
|---|---|---|
| CMake | 3.28 or newer | `tools/verification-common.ps1` |
| MSVC | Visual Studio 2022 x64 tools | `CMakePresets.json` |
| Qt | 6.9, MSVC 2022 64-bit kit | `tools/verification-common.ps1` |
| Node.js | 22 or newer | `tools/verification-common.ps1` |
| Python | 3.12 for CI quality tooling | `.github/workflows/ci.yml` |
| clang-format / clang-tidy | exactly 18.1.8 | `tools/verification-requirements.txt` |
| ffmpeg / ffprobe | 6.1 through major 8.x for the foundation; record the exact pair, with 7.1.1 currently pinned for Windows release evidence | `tools/verification-common.ps1` and the 1.1.0 evidence record |
| nlohmann-json / yaml-cpp | vcpkg manifest plus pinned baseline | `vcpkg.json` and ADR 0002 |

Do not silently substitute a global clang tool or a different dependency baseline. If bootstrap finds a
dirty `.vcpkg` checkout, preserve or remove those local changes before retrying; the script intentionally
refuses to overwrite them.

## Verification Tiers

Run tiers from the repository root. `-Fix` is allowed only with local Quick or the Lint CI lane and
changes formatting in tracked C++ source/test files, so inspect the resulting diff.

```powershell
pwsh -NoProfile -File tools/verify.ps1 -Tier Quick
pwsh -NoProfile -File tools/verify.ps1 -Tier Full
pwsh -NoProfile -File tools/verify.ps1 -Tier Release
```

| Tier | What it proves | When to run |
|---|---|---|
| Quick | Pinned formatting/static policy, Qt-free core+CLI build, fast tests, skill/docs/site contracts | Before every push; default for ordinary changes |
| Full | Quick plus real ffmpeg tests, CLI fixtures, Qt build/model tests, and seeded noninteractive GUI startup | Before merging core, CLI, Qt, or workflow changes |
| Release | Full plus demo capture, version/manifest/document consistency, package assembly, and archive audit | From the intended release commit before dispatching publication |

The optional managed hook runs Quick and changes only this repository's Git directory:

```powershell
pwsh -NoProfile -File tools/install-hooks.ps1
pwsh -NoProfile -File tools/install-hooks.ps1 -Remove
```

## Test Taxonomy

CTest execution labels and resource size answer different questions.

| Axis | Class | Contract |
|---|---|---|
| Runtime | `fast` | Pure or bounded native tests with no real external media process; default local confidence path |
| Runtime | `slow` | Real ffmpeg integration; requires the pinned executable on `PATH` |
| Runtime | `qt` | Qt model/settings tests from the GUI preset; may require a Qt-capable environment |
| Resource | none | No binary fixture; inline values or small text/JSON/YAML only |
| Resource | synthetic-small | Runtime-generated, low-resolution, short-duration media; never user media |
| Resource | candidate | Package, deployed site, or clean-archive output; store as a bounded artifact, not a normal unit fixture |

Resource class is documentation metadata, not an additional CTest label. The canceled VID-43 scope is not
reintroduced here. If a test needs a candidate-sized resource, keep it out of `fast`, generate it
deterministically, bound its duration/size, and retain it only when failure or release evidence requires it.

Run labels directly when diagnosing a tier:

```powershell
ctest --test-dir build/core-release -C Release -L fast --output-on-failure
ctest --test-dir build/core-release -C Release -L slow --output-on-failure
ctest --test-dir build/windows-gui-release -C Release -L qt --output-on-failure
```

## Quality Gates

### Formatting and static analysis

The Lint lane calls `tools/verify.ps1 -CiLane Lint`. It checks every tracked `.cpp` and `.hpp` under
`src/` and `tests/`, enforces the pinned formatter, runs deterministic text/review policies, rejects Qt
includes in `src/core` and `src/cli`, and analyzes core translation units with clang-tidy on Ubuntu.
Windows verifies the pinned clang-tidy configuration; the Ubuntu job is authoritative for full core
translation-unit analysis.

### Qt-free and Qt boundaries

`core-release` must configure, build, and test with `VIDCHOPPER_BUILD_GUI=OFF`. A shared-service API is
not Qt-free merely because one caller avoids Qt: its public headers, link dependencies, and tests must build
without the Qt SDK. Qt model contracts run under the `qt` label, including `QAbstractItemModelTester`
coverage where applicable.

### Site and agent skill

The Docs lane runs the deterministic skill contract, `npm ci`, frontend tests, typechecks, canonical route
build/audit, a Wrangler dry run, and the GitHub Pages compatibility build. Production acceptance additionally
requires the gated Cloudflare workflow and cache-busted live validation described in
`knowledge/operations/cloudflare-production.md`.

### Internal Markdown and public documentation

Repository Markdown is the authoritative internal documentation source for 1.0. Keep commands, links, and
release contracts accurate in the Markdown itself. Do not add a tracked PDF copy, PDF freshness gate, or new
document-rendering infrastructure for this guide. Presentation work for the public documentation at
`vidchopper.app/docs` is a separate post-1.0 concern and is not a 1.0 release gate.

### Package and archive

`tools/package-windows.ps1` assembles `VidChopper-<version>-windows-x64.zip` from already-built GUI and CLI
inputs, deploys the Qt runtime, includes notices/license and the matching agent skill, rejects a version
mismatch, and does not bundle ffmpeg/ffprobe. `tools/verify-release-archive.ps1` expands that exact ZIP in an
isolated workspace, verifies inventory and CLI/skill versions, performs the ChapterBuilder dry-run/export,
probes rendered outputs, and writes bounded JSON evidence including the archive SHA-256.

## GitHub CI Parity

CI uses path classification to skip only irrelevant jobs. Code paths run Lint, Core, and GUI; docs paths run
Docs; agent-skill paths additionally run deterministic Windows and Ubuntu checks. Superseded PR runs are
canceled by the workflow concurrency group. Never treat a canceled older run as evidence for a newer commit.

| GitHub job | Local reproduction | What GitHub adds |
|---|---|---|
| `changes` | `git diff --name-only origin/main...HEAD`, then compare with `.github/workflows/ci.yml` filters | `dorny/paths-filter` event context |
| `lint` | `pwsh -NoProfile -File tools/verify.ps1 -CiLane Lint` | Ubuntu 24.04, GCC 13, cached pinned Python tools |
| `core-tests` | `pwsh -NoProfile -File tools/verify.ps1 -CiLane Core` | Clean Windows 2022, cached vcpkg, ffmpeg 7.1.1 |
| `gui-build` | `pwsh -NoProfile -File tools/verify.ps1 -CiLane Gui` | Clean Windows 2022, cached Qt 6.9/vcpkg, ffmpeg 7.1.1 |
| `docs-check` | `pwsh -NoProfile -File tools/verify.ps1 -CiLane Docs` | Clean Node 22 install and npm cache |
| agent skill matrix | `pwsh -NoProfile -File tools/agent-skill-artifacts.ps1 -Mode Check` | Both Windows and Ubuntu path/ZIP behavior |
| Pages `deploy` | `pwsh -NoProfile -File tools/verify.ps1 -CiLane Docs` reproduces its build/audit | Pages upload, environment, and deployment identity are remote-only |
| Cloudflare `authorize` | No local substitute; inspect the workflow input, ref, and environment configuration | GitHub environment authorization is intentionally remote-only |
| Cloudflare `deploy` | `pwsh -NoProfile -File tools/verify.ps1 -CiLane Docs` reproduces pre-deploy checks | Credential preflight, mutation, identity correlation, and HTTPS acceptance are remote-only |
| Release `package-candidate` | `pwsh -NoProfile -File tools/verify.ps1 -Tier Release` | Clean Windows packaging and immutable artifact upload |
| Release `smoke-clean-archive` | `pwsh -NoProfile -File tools/verify-release-archive.ps1 -Version <version> -ArchivePath <zip>` | A second clean Windows runner and retained JSON evidence |
| Release publish job | No local substitute; verify metadata/digest inputs before approving the protected job | GitHub tag/release mutation and remote asset re-download |

Each CI lane writes a log under `artifacts/ci` and uploads the last bounded diagnostic section only on
failure. Download that failure artifact, reproduce the same `-CiLane` locally, and fix the first coherent
failure class. Cache hits are performance optimizations, not evidence; scripts still validate pinned versions
and the candidate itself.

## Synthetic Fixture Policy

- Use `ffmpeg` `lavfi` sources for deterministic, bounded, low-resolution media.
- Keep public structured fixtures game-neutral and free of user/customer media or private paths.
- Check in small JSON/YAML/text contracts; generate binary media at test time.
- Pin duration, dimensions, frame rate, pixel format, and command arguments so probes are comparable.
  The clean-archive ChapterBuilder smoke is an explicit long-timeline exception: it generates 11,392
  seconds at 160x90 and 1 fps to cover the public chapter ranges while keeping processing cost bounded.
- Give new external-process wrappers a process-level timeout and retain stdout/stderr only within
  bounded diagnostics. The current release smoke still uses direct PowerShell invocations, so the
  Release workflow explicitly bounds every job with `timeout-minutes` until those wrappers are
  migrated. A timeout is a failed gate, never a reason to publish.
- Delete ordinary generated workspaces through existing scoped cleanup; retain a failing/release candidate only
  as an explicit artifact.

A flaky test is a product defect in the evidence system. Do not add blind retries or rerun until green. Fix
the race/environment contract, or quarantine it visibly with an owner and a separate follow-up; a quarantined
test cannot satisfy a release gate.

## Version and Channel Policy

The issue wording refers to earlier "0.3 alpha" and "1.0 beta" plans. The repository's actual published
sequence is authoritative:

| Line | GitHub state | Meaning |
|---|---|---|
| `0.1.x-alpha`, `0.2.0-alpha` | prerelease | Early desktop/product experiments |
| `0.3.0-beta` | prerelease | First complete portable GUI+CLI beta |
| `1.0.0` | stable release | Windows 10/11 x64 stable boundary on the shared engine |
| `1.1.0` | stable release | Windows 10/11 x64 release with the shared Unix foundation |

Do not relabel history to fit obsolete shorthand. The release workflow now publishes through the
protected stable path without `--prerelease` and asserts `isPrerelease == false` for stable releases.

### Version-bump checklist

1. Set both `project(VERSION ...)` and `VIDCHOPPER_DISPLAY_VERSION` in `CMakeLists.txt`;
   the numeric project version and user-facing channel label are separate contracts.
2. Set the numeric base in `vcpkg.json` (`version-semver`) and the display version in
   `docs/package.json` plus its lockfile root package entries.
3. Update required release/download constants, package README text, release metadata/notes, and versioned
   agent-skill routes/artifacts. Public documentation presentation at `vidchopper.app/docs` is tracked
   separately after 1.0.
4. Search the whole tree for the old display version and classify every retained historical reference.
5. Run the deterministic agent-skill artifact generator/check if versioned bytes changed.
6. Run Quick, Full, then Release from the intended commit; never build release bytes from a dirty checkout.

## Candidate, Publication, and Remote Verification

### Candidate checklist

1. Confirm every non-canceled roadmap blocker is Done on `main` and record any explicit deferral.
2. Confirm the checkout is clean and `HEAD` is the reviewed release commit.
3. Run `tools/bootstrap.ps1 -CheckOnly`, then `tools/verify.ps1 -Tier Release`.
4. An optional `publish=false` dispatch is a rehearsal of the commit and workflow only. Record its
   evidence, but do not approve its ZIP for a later run because a new ZIP can have different bytes.
5. Before the real stable dispatch, VID-44 must put the publish job behind a protected
   `release-environment` approval (or add a separate promotion workflow that accepts an immutable
   source run/artifact identity). The current automatic publish path is not an acceptable stable gate.
6. Dispatch the real run once. Let that run package and smoke the candidate, then pause before
   publication. Record its artifact name, SHA-256, clean-runner evidence, and release commit.
7. Compare the clean-runner archive identity with the package artifact from that same run.

### Publication checklist

1. Confirm the target tag and GitHub release do not already exist; never overwrite release history.
2. Obtain explicit human approval inside the paused job for that same run's commit, version, channel,
   candidate digest, and clean-runner evidence. Do not redispatch and rebuild after approval.
3. Approve the protected publish job (or promote the immutable artifact by run/artifact ID).
4. For every stable release, verify the stable workflow path omits `--prerelease` and checks non-draft,
   non-prerelease metadata.
5. Record the tag, commit, release URL, asset URL, checksum URL, and workflow run in Linear and the knowledge
   publishing record.

### Remote acceptance checklist

1. Download the ZIP and `.sha256` from the release URL, not from the local workspace.
2. Verify the downloaded SHA-256 equals the proven candidate digest.
3. Run `tools/verify-release-archive.ps1` against the downloaded archive on clean supported Windows.
4. Confirm the GUI seeded startup, Qt-free CLI, version/help/direct/chop modes, ChapterBuilder export,
   manifests, and ffprobe output evidence.
5. Verify public download links resolve to the stable tag and asset, then cache-bust the live download checks.
   Public documentation presentation at `vidchopper.app/docs` remains separate post-1.0 work.

### Rollback and correction

Stop publication before mutation whenever a gate fails. If a GitHub release is already public, do not replace
assets under the same tag or silently move the tag. For an ordinary defect, preserve evidence and publish a
corrected patch version from a new reviewed commit/tag. For secret or private-file exposure, first preserve
only hashes and necessary evidence in a restricted location, then immediately make the public release/asset
unavailable (convert it to draft or remove the affected asset/release under the incident rollback authority),
rotate every exposed credential, and verify the public URLs no longer serve the bytes. Record any tag
disposition explicitly; never move it silently. For the canonical site, also use the version-identity rollback
procedure in `knowledge/operations/cloudflare-production.md`.

## Evidence Before Completion

Humans and agents use the same bar:

- State exactly which local commands ran, on which commit, and which were not run.
- Do not claim GitHub checks passed until the relevant run reports success for the reviewed head SHA.
- Do not claim a release passed from a local ZIP; correlate the uploaded/downloaded bytes by SHA-256.
- Do not paste secrets, tokens, private paths, or unbounded logs into issues or PRs.
- Do not weaken lint/tests, hide a canceled run, retry a flaky test into green, or substitute another lane.
- Keep publication, production mutation, destructive cleanup, and rollback behind their explicit human gates.
- Update Linear by stable issue ID with branch, PR, commit, run URLs, acceptance evidence, and any residual risk.

Completion means the requested acceptance contract is satisfied and evidenced. A prepared command, draft PR,
queued workflow, or locally passing substitute is progress, not completion.
