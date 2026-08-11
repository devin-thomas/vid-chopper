# Released CLI contract

Use this reference for invocation, optional flags, settings precedence, batch pairing, and failures.
Do not document proposed flags as released behavior.

## Contents

- [Invocation forms](#invocation-forms)
- [Released flags](#released-flags)
- [Settings and outputs](#settings-and-outputs)
- [Directory pairing](#directory-pairing)
- [Exit and failure handling](#exit-and-failure-handling)

## Invocation forms

Use exactly one explicit chapter source:

```text
VidChopperCLI.exe <input-video> <chapters.json|chapters.yaml> [options]
VidChopperCLI.exe <input-video> --embedded [options]
VidChopperCLI.exe chop <input-video> <chapters.json|chapters.yaml> [options]
```

Quote absolute Windows paths in PowerShell. The direct and `chop` forms have the same chapter-config
behavior. Use `--embedded` only after the user selects detected embedded chapters.

## Released flags

| Flag | Value | Released behavior |
| --- | --- | --- |
| `--embedded` | none | Explicitly use chapters embedded in each input video. |
| `--dry-run` | none | Probe and print the complete plan without settings, output, manifests, or clips. |
| `--config` | path | Use the supplied file as the sole CLI settings store for this run. |
| `--config-path` | path | Alias for `--config`. |
| `--portable` | none | Use the deterministic settings sidecar beside the executable. Do not combine with `--config`. |
| `--crf` | `0..51` | Override x264 CRF for this run. |
| `--cq` | `0..51` | Override NVENC CQ when that encoder is actually selected. |
| `--preset` | name | Apply the current encoder preset override. |
| `--threads` | `0..255` | Override ffmpeg threads; `0` uses ffmpeg's default. |
| `--aggregate-json` | path | Write one atomic run-level JSON manifest. |
| `--aggregate-csv` | path | Write one atomic run-level CSV manifest. |
| `--stop-on-first-error` | none | Stop batch execution after the first failed item. |
| `--use-gui-config` | none | Explicitly import `VidChopper.ini` before CLI-owned settings. |
| `--version` | none | Print `VidChopperCLI 1.0.0`. |
| `--help` | none | Print help; `-h` is the short form. |

There is no released `--existing-output` or output-directory flag. Never invent either. Use `--crf`
unless the effective plan selects NVENC; `--cq` alone does not prove NVENC selection.

## Settings and outputs

Resolve the CLI settings location first. The default is the native platform location; `--config` and
`--config-path` select one explicit settings file, while `--portable` selects the deterministic sidecar
beside the executable. Explicit and portable modes cannot be combined. An explicit settings file is the
sole CLI settings store and does not import GUI settings.

```powershell
& "C:\Tools\VidChopper\VidChopperCLI.exe" `
  "C:\Media\event.mp4" "C:\Media\event.chapters.json" `
  --config "C:\Media\event.cli.ini" --dry-run

& "C:\Tools\VidChopper\VidChopperCLI.exe" `
  "C:\Media\event.mp4" "C:\Media\event.chapters.json" `
  --portable --dry-run
```

Resolve settings from lowest to highest precedence:

1. built-in CLI defaults;
2. optional `VidChopper.ini`, only with `--use-gui-config`;
3. CLI-owned `VidChopperCLI.ini`;
4. ChapterFile `output` and `encoder` fields; and
5. explicit CLI flags.

Dry-run reads settings but creates nothing. A normal run may create `VidChopperCLI.ini` beside
`VidChopperCLI.exe`. It never writes the GUI file. Unknown CLI INI keys are ignored for forward
compatibility.

The `1.0.0` CLI default is overwrite mode. Dry-run renders the effective mode and reports every planned
path as `Existing output: yes|no`. Block on `yes`, recheck paths after approval, and prefer a fresh
output folder. `output.folder` is sanitized into a sibling folder of the source.

Each job writes `vidchopper-manifest.json` by default. Settings may also enable per-job CSV. The
aggregate flags add run-level manifests; their writes are atomic and failures make the CLI nonzero.

## Directory pairing

Inventory the whole directory before deciding the mode. Supported modes are:

- N:1: a source directory plus one shared ChapterFile;
- N:N: a source directory plus ChapterFile directory, paired by case-insensitive exact stem; or
- embedded: a source file/directory plus `--embedded`.

Supported source extensions are `.mp4`, `.mkv`, and `.mov`. Supported ChapterFile extensions are
`.json`, `.yaml`, and `.yml`. Reject 1:N, duplicate stems, count mismatches, missing pairs, orphans,
and partial scans. In an embedded directory batch, the CLI may skip sources without chapters; report
those skips explicitly.

## Exit and failure handling

| Code | Meaning | Required response |
| ---: | --- | --- |
| `0` | Successful command | Verify planned files and manifests before success. |
| `1` | Usage, path, ChapterFile, or plan validation failure | Preserve the error, correct only approved input, and dry-run again. |
| `2` | Dry-run path inspection, ffmpeg/export, or manifest failure | Withhold overall success and inventory preserved successful clips. |
| `3` | ffprobe failure or ffmpeg failed start | Report tool, source, process state, exit detail, timeout/crash context. |

Preserve bounded stderr and the numeric exit. Distinguish failed start, timeout, crash, and nonzero
tool exit. Never translate ChapterBuilder data or silently retry with different flags. When a batch
continues after a failure, verify each job independently rather than flattening it into one result.
