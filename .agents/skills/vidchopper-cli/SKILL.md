---
name: vidchopper-cli
description: Plan, review, export, and verify local video chapter clips with VidChopperCLI and JSON/YAML ChapterFiles, including ChapterBuilder exports and embedded chapters. Use for read-only media inspection, safe dry-runs, explicit collision and write confirmation, local clip export, or manifest verification on Windows.
license: MIT
metadata:
  vidchopper.skill-contract-version: "1"
  vidchopper.cli-version: "0.3.0-beta"
  vidchopper.chapterfile-schema-version: "1"
  vidchopper.export-manifest-schema-version: "1"
---

# VidChopper CLI

Keep the source video, ChapterFile, prompt, local paths, clips, and manifests on the user's machine.
Use a local-capable Windows harness. Never upload media to bridge a remote runtime limitation.

## Protect the boundary

- Treat inspection and `--dry-run` as read-only. Do not install software, edit `PATH`, create settings,
  export clips, overwrite files, delete files, publish, or upload without the relevant confirmation.
- Require absolute, quoted Windows paths. Reject ambiguous or partially inventoried directories.
- Require one explicit chapter source: a JSON/YAML ChapterFile or user-selected embedded chapters.
- Ask for a new confirmation if the plan, output paths, collisions, or command changes after approval.
- Never claim overall success from an exit code alone. Verify every planned output and manifest.

## Match the supported contract

Require this exact tuple before following the workflow:

- `VidChopperCLI 0.3.0-beta`
- ChapterFile schema `1`
- export-manifest schema `1`
- skill contract `1`

Run only read-only prerequisite checks:

```powershell
& "C:\Tools\VidChopper\VidChopperCLI.exe" --version
ffmpeg -version
ffprobe -version
```

Report the detected CLI, ffmpeg, and ffprobe versions. FFmpeg `7.1.1` is the clean-release evidence;
other versions are outside that exact proof and are not silently rejected. Ask before downloading or
installing any missing tool. Use `--crf` in examples; do not promise that `--cq` selects NVENC in the
`0.3.0-beta` standalone CLI.

## Inspect inputs read-only

Confirm that each source and ChapterFile is readable before invoking the CLI. Use `ffprobe` to inspect
duration, streams, frame rate, and embedded chapters without writing media. For directories, inventory
the complete directory before probing and allow only:

- N sources to one shared ChapterFile; or
- N sources to N ChapterFiles paired by exact filename stem.

Reject one source to multiple ChapterFiles, duplicate stems, missing pairs, and orphan ChapterFiles.
Read [references/chapterfile.md](references/chapterfile.md) when validating schema, timestamps,
ChapterBuilder handoff, directory pairing, or embedded chapters.

## Preserve ChapterBuilder data

Use a ChapterBuilder-produced JSON/YAML ChapterFile unchanged. Do not translate it into an invented
shape. ChapterBuilder is unnecessary when the user already has its exported file. If the user asks to
obtain ChapterBuilder, use only `devin-thomas/ChapterBuilder`, check Releases live, and ask before any
download or source build.

## Plan before writing

Run one supported form with `--dry-run`:

```powershell
& "C:\Tools\VidChopper\VidChopperCLI.exe" `
  "C:\Media\event.mp4" `
  "C:\Media\event.chapters.json" `
  --dry-run

& "C:\Tools\VidChopper\VidChopperCLI.exe" chop `
  "C:\Media\event.mp4" `
  "C:\Media\event.chapters.json" `
  --dry-run

& "C:\Tools\VidChopper\VidChopperCLI.exe" `
  "C:\Media\meeting.mkv" `
  --embedded `
  --dry-run
```

Require a successful plan with `Planned chapters: N`. Confirm that no `VidChopperCLI.ini`, output
directory, clip, or manifest was created. Capture the chapter source, effective settings, exact output
directory, every `Segment:` path, every `Existing output:` value, manifest paths, and planned ffmpeg
commands. Treat `output.folder` as a sanitized sibling-folder pattern, not an arbitrary destination.

## Gate the export

Present one compact review containing:

- detected CLI, ffmpeg, ffprobe, ChapterFile schema, and skill versions;
- source and chapter-source paths, provenance, and chapter count;
- exact output directory, every planned file, and all existing destinations;
- effective overwrite behavior and per-job or aggregate manifest locations;
- the `VidChopperCLI.ini` path that a normal run may create beside the executable; and
- the exact command that will run without `--dry-run`.

Stop on every `Existing output: yes`. The beta CLI has no explicit safe overwrite flag and normally
uses overwrite mode. Prefer a fresh sibling output folder; otherwise require explicit approval for
each listed overwrite. A broad approval does not authorize unlisted paths. Immediately before export,
recheck every destination. Abort and request fresh approval if any destination appeared or changed.

## Export after confirmation

After explicit approval, run the reviewed command with only `--dry-run` removed. Do not add unsupported
flags. Stream bounded progress and preserve stdout, stderr, and the exit code. Read
[references/cli.md](references/cli.md) before using optional flags, settings, batch modes, or diagnosing
tool failures.

## Verify the result

Require all of the following before reporting success:

1. The summary's exported, skipped, and failed counts match the reviewed plan.
2. Every planned clip exists at its exact path.
3. Every expected `vidchopper-manifest.json` exists and has `jobStatus: success`.
4. The segment and output counts equal the dry-run chapter count, with every `processState: success`.
5. `ffprobe` reports each clip duration within one second of its planned duration.
6. Requested aggregate JSON/CSV manifests exist and agree with the job results.

Read [references/manifests.md](references/manifests.md) for field checks, partial-failure handling, and
the final report. Do not delete partial outputs or retry writes without a new decision.

## Handle failures exactly

- Exit `0`: verify files and manifests before reporting success.
- Exit `1`: report the usage or validation error; change only user-approved paths/config, then dry-run.
- Exit `2`: withhold overall success; report dry-run path-inspection errors or verify individually
  preserved clips and manifest errors.
- Exit `3`: report the executable, source, process state, and bounded ffprobe/ffmpeg-start error.

Never hide a nonzero exit behind partial success. A manifest-write failure may leave valid clips; keep
and report them rather than deleting them.

## Work offline or stop on mismatch

Prefer a compatible copy bundled with a future release, then a previously verified local copy, then
the stable `https://vidchopper.app/agents/vidchopper-cli/SKILL.md` copy. The already-published
`v0.3.0-beta` application ZIP does not contain this skill. Verify the complete exact-version tuple and
SHA-256 metadata before replacing a trusted cached copy.

When offline, use this skill's bundled schema and examples. If no compatible verified copy exists,
stop and point to local repository documentation; do not invent commands or claim onboarding is
complete. Treat same-origin digests as transfer-integrity checks, not publisher identity.

## Load only what is needed

- Read [references/cli.md](references/cli.md) for released flags, settings, directories, and exits.
- Read [references/chapterfile.md](references/chapterfile.md) for schema and ChapterBuilder inputs.
- Read [references/manifests.md](references/manifests.md) for post-export verification.
- Use `assets/chapter-config.schema.json` and the bundled examples for offline validation and practice.
