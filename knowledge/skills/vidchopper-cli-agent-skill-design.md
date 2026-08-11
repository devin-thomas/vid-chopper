# VidChopper CLI Agent Skill Design

This is the VID-48 UX and versioning decision for a first-party `vidchopper-cli` Agent Skill. It is
an implementation contract for VID-51, VID-52, and VID-53, not the skill itself.

Decision checkpoint: 2026-08-01, against `origin/main` commit `07aba676`.

## Baseline and boundaries

- Product baseline: `VidChopperCLI 1.0.0`.
- ChapterFile schema baseline: version `1` JSON or YAML.
- Export-manifest schema baseline: version `1`.
- Compatibility evidence: the released CLI plans and exports all 16 chapters in the byte-identical
  ChapterBuilder TNS 2XKO #36 fixture.
- The agent runs local `VidChopperCLI.exe`, `ffprobe`, and `ffmpeg`. The site never receives media,
  ChapterFiles, prompts, paths, manifests, or clips.
- VID-51 prepares canonical docs and generic raw-asset staging. VID-52 implements and populates the
  skill and its manifests. VID-53 adds the homepage onboarding prompt. VID-55 owns production
  publication.
- This design does not authorize automatic installation, overwrite, deletion, upload, publishing,
  spending, deployment, DNS changes, or other external mutations.

## Decision

The checked-in skill is the canonical source. The hosted and packaged forms are verified products of
that source, not separately maintained copies.

| Surface                      | Contract                                                                      |
| ---------------------------- | ----------------------------------------------------------------------------- |
| Canonical repository source  | `.agents/skills/vidchopper-cli/SKILL.md`                                      |
| Stable hosted entry point    | `https://vidchopper.app/agents/vidchopper-cli/SKILL.md`                       |
| Stable hosted metadata       | `https://vidchopper.app/agents/vidchopper-cli/manifest.json`                  |
| Immutable hosted snapshot    | `https://vidchopper.app/agents/vidchopper-cli/v1.0.0/SKILL.md`           |
| Immutable hosted metadata    | `https://vidchopper.app/agents/vidchopper-cli/v1.0.0/manifest.json`      |
| Versioned discovery bundle   | `https://vidchopper.app/agents/vidchopper-cli/v1.0.0/vidchopper-cli.zip` |
| Future portable ZIP fallback | `.agents/skills/vidchopper-cli/` unchanged, beside a small integrity manifest |
| Human documentation          | `https://vidchopper.app/docs`                                                 |
| Optional discovery index     | `https://vidchopper.app/.well-known/agent-skills/index.json`                  |

The stable URL follows the currently supported released CLI. The versioned URL never changes after a
release. The release ZIP copy is the preferred offline match for the CLI in that same archive.

The optional well-known index is additive discovery, not a prerequisite. Cloudflare's
[`agent-skills-discovery-rfc`](https://github.com/cloudflare/agent-skills-discovery-rfc) is still a
proposal, so the promised direct URL remains usable by generic harnesses that do not implement the
index.

## Short UX brief

An agent starts from a local video and either an existing ChapterFile or an approved ChapterBuilder
handoff. It selects only an exact-version skill copy, checks the local CLI and media tools without
changing the machine, validates the ChapterFile, and runs `--dry-run`. It then presents one compact
review containing versions, chapter provenance, planned commands, every output path, collisions, and
the settings file that a real run may create. Export begins only after explicit confirmation. The
agent immediately rechecks planned paths, runs locally, verifies clips and manifests, and reports
bounded failures without uploading media or claiming partial work as overall success.

The remaining sections are the evidence and executable downstream contract behind this brief. They
make VID-51 through VID-53 testable without turning VID-48 into a second roadmap.

## Current surface map

| Concern                   | Current source of truth                                                                   | Skill/distribution use                                     |
| ------------------------- | ----------------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| Released CLI syntax       | `src/cli/cli_arguments.cpp` and packaged `--help`                                         | Examples and flag-drift test                               |
| Exit behavior             | `src/cli/cli_app.hpp` and `src/cli/cli_app.cpp`                                           | Error classification and retry rules                       |
| ChapterFile contract      | `docs/cli-config-schema.md` and `docs/schemas/chapter-config.schema.json`                 | Focused schema reference and hosted schema route           |
| Settings precedence       | `docs/cli-settings.md` and `src/cli/cli_settings.cpp`                                     | Effective-setting and overwrite warnings                   |
| Safe starter examples     | `examples/chapter-config.json` and `examples/chapter-config.yaml`                         | Small skill examples or canonical pointers                 |
| ChapterBuilder proof      | `tests/fixtures/chapterbuilder/tns-2xko-36-chapters.json`                                 | Unmodified compatibility smoke fixture                     |
| Clean release proof       | `tools/verify-release-archive.ps1` and `.github/workflows/release.yml`                    | Packaged skill/example execution gate                      |
| Portable package notes    | `packaging/windows/README.txt`                                                            | Offline skill pointer and prerequisites                    |
| Release identity/evidence | `packaging/releases/1.0.0.md` and `knowledge/operations/publishing-and-workflows.md` | Version and digest alignment                               |
| Human entry points        | `README.md` and the React site in `docs/`                                                 | Canonical docs and onboarding links                        |
| Legacy public route       | GitHub Pages with Vite base `/vid-chopper/` and hash routing                              | Intentional migration landing, not canonical skill hosting |

VID-52 adds the repository skill without duplicating these contracts by hand. References either reuse
the source files during packaging or are generated and checked against them.

The canonical schema and starter examples identify
`https://vidchopper.app/schemas/chapter-config/v1/schema.json`, and VID-51 also publishes a stable
alias. The byte-identical ChapterBuilder fixture still identifies the legacy `vidchopper.dev` route
and remains unchanged until its upstream export changes; that `$schema` string is treated as
provenance, not fetched by the CLI.

## Why this shape

The [Agent Skills specification](https://agentskills.io/specification) defines a skill as a directory
with a required `SKILL.md`, optional focused resources, and progressive disclosure. Cloudflare's
[skills repository](https://github.com/cloudflare/skills) demonstrates cross-harness installation,
while Cloudflare's [Agents skill runtime](https://developers.cloudflare.com/agents/runtime/execution/agent-skills/)
loads catalog metadata before the full instructions and resources.

Cloudflare labels that entire runtime/source API experimental, with script execution especially early.
VidChopper takes its file-format contract from the independent Agent Skills specification and treats
Cloudflare's runtime behavior as design evidence rather than a stable dependency.

The base specification standardizes skill contents, not remote distribution or version locking. Its
[client integration guidance](https://agentskills.io/client-implementation/adding-skills-support)
recommends `.agents/skills` as a cross-client project location, while the stable URL, immutable
snapshot, release co-bundling, and checksum rules below are VidChopper's distribution contract.

VidChopper keeps the useful parts of that pattern and tightens the product-specific gaps:

- one short URL can onboard a generic agent without first choosing an installer;
- the repository, hosted snapshot, and release package are digest-checked from one source;
- an immutable versioned URL makes an old CLI workflow reproducible;
- the release package works offline without silently refreshing instructions;
- the main skill stays small and loads detailed CLI/schema/manifest references only when needed;
- local-media privacy and human confirmation are part of the procedure, not a separate warning page.

The tradeoff is additional release synchronization and route validation. A generic harness may still
need the user to paste the URL or copy the directory into its own skill location. That is preferable
to a VidChopper-specific installer or remote execution service in the first version.

| Aspect       | Cloudflare-inspired pattern                                                                                    | VidChopper decision and tradeoff                                                                               |
| ------------ | -------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------- |
| Discovery    | Central skills repository plus runtime-specific install paths                                                  | Keep those options, but add one product-owned URL that a generic agent can read directly.                      |
| Loading      | Name/description discovery, then full instructions and on-demand resources                                     | Use the same progressive-disclosure shape and keep the entry file below 500 lines.                             |
| Updates      | Repository/plugin updates and, in Cloudflare's runtime, optional remote skill sources                          | Stable alias plus immutable versioned snapshots; more release work, but reproducible old CLI behavior.         |
| Offline use  | Clone or copy into each harness's skill directory                                                              | Ship a byte-matched copy with the CLI so media work does not depend on network availability.                   |
| Scripts      | Supported by some runtimes; Cloudflare labels its full skill runtime experimental and scripts especially early | Do not require scripts in version 1; any later helper is non-mutating and separately gated.                    |
| Integrity    | Cloudflare's discovery proposal includes SHA-256 artifact digests                                              | Generate digests now, but keep direct URL/manual install compatibility because discovery is not final.         |
| Capabilities | Skills can be paired with broad MCP/tool access                                                                | Stay skill-first over the local CLI; MCP remains a separate VID-54 decision.                                   |
| Safety       | Runtime permissions vary                                                                                       | Put local-media privacy, collision review, and consequential-action confirmation in the skill workflow itself. |

A hosted-only agent that cannot access the user's filesystem or run a Windows executable may explain
the contract or help draft a ChapterFile, but it must hand execution to a local-capable harness. It
must not ask the user to upload private media merely to bridge that runtime limitation.

## Target journey: video to verified clips

### 1. Establish the local boundary

The agent identifies the source video and desired chapter source, states that all media work stays
local, and asks before installing software or changing configuration. It never uploads the source or
includes private paths in URLs, analytics, issue comments, or remote logs.

### 2. Select a compatible skill copy

Use copies in this order:

1. A copy bundled with a post-VID-52 VidChopper release when its metadata matches
   `VidChopperCLI.exe --version`.
2. A previously verified local copy whose declared CLI and schema versions match exactly.
3. The stable HTTPS copy when network access is available and a refresh is needed.

Version 1 uses exact-match compatibility only: CLI `1.0.0`, ChapterFile schema `1`, export
manifest schema `1`, and skill contract `1`. A cached or hosted skill with any different tuple is not
compatible. Supported ranges can be introduced only after CI exercises a real version matrix and the
manifest gains an unambiguous machine-readable range contract.

The `v1.0.0` ZIP contains the exact versioned skill and adjacent manifest. The historical
`v0.3.0-beta` ZIP contains no skill or schema copy; existing beta installations use a verified
repository or hosted copy. If they are offline without that cache, stop and provide the local human
docs/pinned schema path rather than claiming agent onboarding is complete.

When fetching, accept only the exact `https://vidchopper.app` origin, record the resolved skill and
CLI versions, and verify SHA-256 against release or discovery metadata. Do not replace a trusted local
copy when the expected digest is unavailable or mismatched. Fall back to the compatible local copy;
otherwise stop and explain the mismatch instead of inventing commands.

Origin trust and content digests are separate. For a packaged copy, first trust the immutable GitHub
Release under `devin-thomas/vid-chopper`, verify its externally published ZIP checksum, and then use
the internal manifest to detect copy corruption. For an online refresh, the user/harness must first
trust or allowlist the HTTPS origin. It then reads the stable `manifest.json`, resolves the exact
`sourceCommit`, and compares the hosted bytes with the canonical file at that commit in the allowlisted
`devin-thomas/vid-chopper` repository. The stable manifest and stable skill must resolve to the same
immutable versioned manifest and skill. Same-origin manifest or index digests verify transfer and
cache consistency, not publisher identity. VidChopper does not claim a cryptographic signing chain
that the release process does not yet provide.

### 3. Verify prerequisites without changing the machine

The entry flow checks:

```powershell
& "C:\Tools\VidChopper\VidChopperCLI.exe" --version
ffmpeg -version
ffprobe -version
```

The released clean-runner proof uses FFmpeg `7.1.1`. Other versions are not silently rejected, but the
agent reports the detected versions and explains that they are outside that exact release evidence.
Missing tools, downloads, PATH edits, or ChapterBuilder installation require user confirmation.

The `1.0.0` CLI does not itself perform the desktop GPU-detection step. Skill examples therefore use
`--crf` unless the effective plan selects NVENC and do not promise that `--cq` selects it.

The authoritative ChapterBuilder source is
[`devin-thomas/ChapterBuilder`](https://github.com/devin-thomas/ChapterBuilder). At this design
checkpoint it has no published GitHub Release, despite its README describing the intended release
shape. The skill checks Releases live before offering a download, verifies published checksums when
one exists, and otherwise offers a source build only after explicit approval. A user who already has a
ChapterBuilder-produced ChapterFile does not need ChapterBuilder installed to run VidChopper.

### 4. Inspect source and chapter inputs read-only

The agent confirms explicit absolute input paths, checks that they are readable supported files or
directories, and uses `ffprobe` to inspect duration, streams, frame rate, and embedded chapters.
Inspection must not create output or modify media.

The single-source path is the primary onboarding journey. When the user supplies directories, the
agent inventories the complete directory before probing, supports the released N:1 shared
ChapterFile and strict stem-matched N:N modes, rejects 1:N, and applies the output/collision gate to
every planned job. It never draws missing/orphan conclusions from a partial directory scan.

Chapter selection is explicit:

- use a JSON/YAML ChapterFile when one is supplied or produced by ChapterBuilder;
- use `--embedded` only after the user chooses the discovered embedded chapters;
- never infer a missing flag or translate ChapterBuilder data into an undocumented shape;
- validate ChapterFile schema version `1`, unknown fields, timestamp ordering, overlap, and source
  duration before export.

`output.folder` is a sanitized sibling-folder pattern, not an arbitrary destination path. The agent
reports the resolved absolute sibling path from dry-run and does not promise unsupported output
placement.

### 5. Plan with a dry-run

Use quoted absolute paths. Both supported command styles may be shown, but one is sufficient for the
user's run:

```powershell
& "C:\Tools\VidChopper\VidChopperCLI.exe" `
  "C:\Media\event.mp4" `
  "C:\Media\event.chapters.json" `
  --dry-run

& "C:\Tools\VidChopper\VidChopperCLI.exe" chop `
  "C:\Media\event.mp4" `
  "C:\Media\event.chapters.json" `
  --dry-run
```

A valid dry-run probes the source, reports chapter provenance and planned commands, lists effective
settings and output paths, and reports `Planned chapters: N`. It does not create
`VidChopperCLI.ini`, output directories, clips, or manifests.

### 6. Present the consequential-action gate

Before export, the agent reports:

- CLI, ffmpeg, ffprobe, and ChapterFile schema versions;
- source and ChapterFile paths;
- chapter count and selected chapter source;
- exact output directory and planned files;
- any existing destination files;
- effective overwrite behavior and manifest locations;
- the `VidChopperCLI.ini` path that a normal run may create beside the executable;
- the exact export command.

Normal `1.0.0` CLI execution uses overwrite mode unless settings override it, and there is no
explicit overwrite flag. Therefore the skill must inspect every planned destination after dry-run and
stop when a collision exists. It may proceed only after the user explicitly confirms the listed
overwrites. The exact dry-run marker `Existing output: yes` always blocks automatic continuation. A
broad approval such as "do it" does not authorize unlisted paths.

Immediately before export, recheck every planned output path after the approval. If any path appeared
or changed since dry-run, abort and present the new collision for a fresh decision. Prefer a fresh
output folder. This narrows, but cannot make atomic, the beta CLI's check-to-write race; the future
explicit overwrite mode described below is the durable fix.

### 7. Export only after confirmation

After an explicit gate, run the same command without `--dry-run`. Do not add unsupported flags. Stream
bounded progress, preserve stderr and exit code, and stop or continue according to the documented
batch setting.

### 8. Verify and report

Success requires more than exit code zero. The agent verifies:

- the CLI summary reports the expected exported/skipped/failed counts;
- every planned clip exists at the reported path;
- `vidchopper-manifest.json` exists for each job;
- manifest `jobStatus` is `success` and every segment has `processState: success`;
- segment count and output-file count match the dry-run chapter count;
- `ffprobe` reports each clip duration within a skill-defined one-second tolerance of its planned
  duration; this strengthens, rather than describes, the released CLI behavior;
- aggregate manifests are checked when `--aggregate-json` or `--aggregate-csv` was requested.

The final report lists produced files, manifests, skipped files, detected versions, and any failures.
An exit-code `2` caused by manifest writing may follow successfully rendered clips; preserve and report
those paths instead of deleting them. The agent does not publish, upload, delete, or clean up media
without a new confirmation.

## Proposed `SKILL.md` contract

The entry file follows the Agent Skills frontmatter contract and stays below 500 lines. Detailed
material moves to one-level-deep references.

```yaml
---
name: vidchopper-cli
description: Plan and export local video chapter clips with VidChopperCLI and ChapterBuilder ChapterFiles. Use for safe media inspection, dry-runs, confirmed exports, and manifest verification.
license: MIT
compatibility: Windows 10/11 x64; VidChopperCLI 1.0.0; local ffmpeg and ffprobe; network optional.
metadata:
  vidchopper.skill-contract-version: "1"
  vidchopper.cli-version: "1.0.0"
  vidchopper.chapterfile-schema-version: "1"
---
```

Do not use `allowed-tools` in version 1. That field and script execution vary by runtime, and the core
workflow needs only the user's existing shell/file tools. If VID-52 adds a helper script, it must be a
non-mutating validator by default, document dependencies, and require the harness's normal execution
approval.

The body must contain, in this order:

1. local-media and human-gate rules;
2. version/prerequisite checks;
3. source and ChapterFile discovery;
4. ChapterBuilder handoff without format conversion;
5. direct and `chop` dry-run examples;
6. collision review and export confirmation;
7. export and manifest verification;
8. exit-code/error handling;
9. offline/version-mismatch behavior;
10. links to only the references needed for the current task.

Exit-code handling is explicit:

| Code | Meaning                                                                   | Agent response                                                                           |
| ---: | ------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- |
|  `0` | Success                                                                   | Verify files and manifests before reporting success.                                     |
|  `1` | Usage or validation failure                                               | Fix paths/config only from the error; dry-run again.                                     |
|  `2` | Export/manifest failure, including ffmpeg nonzero exit, timeout, or crash | Verify and report individually preserved successful clips, but withhold overall success. |
|  `3` | Any ffprobe failure, or ffmpeg failed start                               | Report the executable, source, process state, and bounded error context.                 |

Recommended resources, added only when they reduce entry-file size:

- `references/cli.md`: released flags, exit codes, settings precedence, and failure examples;
- `references/chapterfile.md`: schema version `1`, timestamp rules, and ChapterBuilder handoff;
- `references/manifests.md`: job/segment success checks and aggregate-manifest behavior;
- `examples/`: a small safe ChapterFile plus the game-neutral ChapterBuilder fixture or a canonical
  pointer with a verified digest.

## Version and integrity contract

One generated manifest records:

```json
{
  "skillContractVersion": 1,
  "cliVersion": "1.0.0",
  "chapterFileSchemaVersion": 1,
  "exportManifestSchemaVersion": 1,
  "skillSha256": "<sha256-of-raw-SKILL.md>",
  "skillArchiveSha256": "<sha256-of-versioned-skill-zip>",
  "chapterFileSchemaSha256": "<sha256-of-raw-schema.json>",
  "sourceCommit": "<release-commit-sha>",
  "repositoryPath": ".agents/skills/vidchopper-cli/SKILL.md",
  "stableUrl": "https://vidchopper.app/agents/vidchopper-cli/SKILL.md",
  "versionedUrl": "https://vidchopper.app/agents/vidchopper-cli/v1.0.0/SKILL.md"
}
```

Release automation generates the digest from the canonical bytes, copies those exact bytes to the
docs artifact and portable ZIP, and fails on mismatch. The hosted stable route uses a short cache
lifetime; versioned routes are immutable. `GET` and `HEAD` return Markdown content types, unknown
skills return `404`, and redirects never cross the canonical origin.

The stable metadata route returns the complete manifest above for the currently supported release.
Its `versionedUrl` and `sourceCommit` must resolve to immutable content. The versioned metadata route
returns byte-identical manifest JSON for that version and is published with immutable caching. A
client rejects a stable skill when either manifest route is missing, the two manifests disagree, the
stable bytes differ from the versioned bytes, or the canonical repository file at `sourceCommit` has
a different digest. Future release packages use the GitHub Release ZIP and adjacent `.sha256` as the
provenance root; their internal manifest remains a consistency check.

Canonical skill artifacts use UTF-8 without a byte-order mark and LF line endings. VID-52 enforces
that path policy with `.gitattributes` (or generates from the Git blob) before hashing, so Windows
checkout conversion cannot make the site and ZIP digests disagree. Digests cover the raw published
bytes, not a platform-normalized local copy.

Schema version `1` will be bundled with the VID-52 skill and future packages; it is not in the
published `v0.3.0-beta` ZIP. Until the canonical site route exists, its immutable network fallback is
the release-commit
[`chapter-config.schema.json`](https://raw.githubusercontent.com/devin-thomas/vid-chopper/886815578066c972de7ef400eb0b6de41a5bc33d/docs/schemas/chapter-config.schema.json),
SHA-256 `e7583c6a9062549059fa224b7adf687096dc940d1fb40a9e39d254805f8299bc`. The
`$schema` field is a hint; `VidChopperCLI.exe` does not fetch it. A mismatched or unavailable network
schema never replaces the compatible bundled copy.

The well-known index uses Cloudflare's proposed discovery shape exactly when published:

```json
{
  "$schema": "https://schemas.agentskills.io/discovery/0.2.0/schema.json",
  "skills": [
    {
      "name": "vidchopper-cli",
      "type": "archive",
      "description": "Plan and export local chapter clips safely with VidChopperCLI.",
      "url": "/agents/vidchopper-cli/v1.0.0/vidchopper-cli.zip",
      "digest": "sha256:<skill-archive-sha256>"
    }
  ]
}
```

The index digest covers the versioned ZIP because the distributable skill has supporting resources.
The direct `SKILL.md` URL remains the readable bootstrap/fallback, and its separate digest remains in
release metadata. Serve the index as `application/json`, the entry file as `text/markdown` or
`text/plain`, and the bundle as `application/zip`; support `GET` and `HEAD`, plus CORS only when a
browser harness needs cross-origin reads. Clients must trust only approved origins and must not
execute downloaded scripts by default.

## End-to-end smoke scenario

The existing release proof is the normative smoke scenario and should be invoked by the skill checks,
not reimplemented with a weaker fixture:

1. Extract the exact candidate release to a clean Windows workspace.
2. Copy `VidChopperCLI.exe` and `yaml-cpp.dll` to an isolated directory to prove the CLI has no Qt
   runtime dependency.
3. Verify `VidChopperCLI.exe --version` equals the release input.
4. Verify `ffmpeg` and `ffprobe` are available; CI installs FFmpeg `7.1.1`.
5. Generate the deterministic 11,392-second, 160x90, one-frame-per-second synthetic source used by
   `tools/verify-release-archive.ps1`.
6. Use `tests/fixtures/chapterbuilder/tns-2xko-36-chapters.json` without edits.
7. Run direct and `chop` dry-runs; each must report `Planned chapters: 16` and create no output.
8. Treat the transition to export as the explicit test gate, then export with the existing CI speed
   overrides `--preset ultrafast --crf 51`.
9. Require `Summary: exported=16`, 16 clips, a successful JSON manifest, 16 successful segments, and
   no failed process state.
10. Probe every output and compare its duration with the manifest's planned duration within a
    one-second tolerance.
11. Record the archive SHA-256, CLI version, skill digest/version, and smoke results in release
    evidence.

This proves the journey with a real ChapterBuilder-produced ChapterFile while avoiding redistribution
or upload of the public VOD.

## Minimum downstream changes

### VID-51: documentation routes

- Make `/docs` and major sections directly reloadable without hash-only routing.
- Publish stable and immutable machine-readable schema, sample, and release-metadata routes; use
  `/schemas/chapter-config/v1/schema.json` for schema version `1`.
- Preserve intentional legacy landing behavior for GitHub Pages/hash URLs.
- Generate physical HTML entries for every supported browser route and use strict custom-`404`
  delivery instead of an unconditional SPA fallback. This keeps direct reloads working without
  turning missing schema, sample, or skill URLs into `200 text/html` responses.
- Validate built-artifact `GET`, `HEAD`, content type, canonical redirects, assets, and `404`
  behavior locally. VID-55 repeats the same contract against its preview and production origins
  after the deployment gate.
- Add a generic raw-asset staging step that can carry later skill artifacts without VID-51 creating or
  owning them. The current Vite build has no `public/` or copy path for schemas, samples, skills, or
  Markdown.

### VID-52: skill and release implementation

- Implement the canonical skill and focused references.
- Narrowly unignore `.agents/skills/vidchopper-cli/**`; preserve the repository's ignore boundary for
  other machine-local `.agents` state.
- Add Agent Skills format validation and extract every documented VidChopper flag for comparison with
  packaged `--help` output.
- Generate and compare repository, hosted, versioned, and packaged digests.
- Populate the stable and versioned skill, manifest, archive, and optional discovery-index artifacts
  through VID-51's generic staging path. VID-55, not VID-52, publishes them to production.
- Bundle the canonical `.agents/skills/vidchopper-cli/` directory and manifest in the portable ZIP so
  compatible project-scanning harnesses can discover the offline copy without a private install path.
- Update CI/Pages path filters so `.agents/skills/vidchopper-cli/**` and its generated raw assets
  trigger the right validation/deployment lanes.
- Exercise all examples plus the ChapterBuilder smoke against the packaged CLI on a clean runner.
- Test discovery with at least two harness styles: a skill-aware directory install and a generic agent
  given only the stable URL.

### Smallest CLI hardening gap

Add an explicit existing-output policy such as
`--existing-output <fail|skip|overwrite>`, make `fail` the agent-facing safe choice, render it in
dry-run output, and record it in manifests. Until that ships, the skill must preflight collisions and
choose a fresh, user-approved sibling output folder, then gate every listed overwrite. The skill must
never document this proposed flag as released behavior.

### VID-53: onboarding prompt

The homepage prompt points to the stable skill URL, requests inspection and dry-run first, and requires
confirmation before export, overwrite, upload, publish, or delete. Download, human docs, and agent
onboarding receive equal visual hierarchy.

## CI and release gates

VID-52 and subsequent releases are guarded by:

- Agent Skills structure/frontmatter validation;
- Markdown link and relative-resource validation;
- CLI help/flag/example drift checks;
- a dry-run assertion that no settings file, output directory, manifest, or clip is created;
- CLI version, ChapterFile schema, skill metadata, site metadata, and package-version alignment;
- distinct checks for product/CLI version, ChapterFile schema version, export-manifest schema version,
  and skill-contract version rather than treating them as one shared number;
- byte/digest equality across repository, site artifact, versioned route, and release ZIP;
- direct/chop dry-run and confirmed export smoke with the ChapterBuilder fixture;
- manifest, output-count, and independent `ffprobe` duration verification;
- built-artifact route and `404` checks;
- cache-busted live checks only after the production gate;
- a privacy assertion that no request or telemetry contains prompts or local media paths.

## Explicit non-goals

- No hosted video processing or media upload.
- No automatic ChapterBuilder or VidChopper installation.
- No silently downloaded or executed scripts.
- No general agent platform.
- No MCP dependency; VID-54 makes that separate evidence-backed decision.
- No claim that local checks prove production publication.
