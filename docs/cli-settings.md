# VidChopper CLI settings

The command-line app has its own settings file so CLI preferences do not mutate or depend on GUI
preferences. The `1.1.0` foundation keeps this separation on every platform.

## Native locations

The file names are stable; the platform adapter chooses the native default root:

| Entry point | Windows | macOS | Linux |
| --- | --- | --- | --- |
| GUI | Existing adjacent `VidChopper.ini` behavior | `~/Library/Application Support/VidChopper/VidChopper.ini` | `$XDG_CONFIG_HOME/VidChopper/VidChopper.ini`, or `~/.config/VidChopper/VidChopper.ini` |
| CLI | Existing adjacent `VidChopperCLI.ini` behavior | Native config root plus `VidChopperCLI.ini` | Native config root plus `VidChopperCLI.ini` |

The current CLI settings API resolves both file names from the executable path and keeps the paths
separate. That is an implementation boundary, not permission to add a new CLI option: the current
`src/cli/cli_arguments.cpp` exposes no `--config` or `--portable` flag.

Portable mode is explicit in the approved foundation contract. A portable executable uses a deterministic
sidecar location; a macOS app uses a file beside the outer `.app` bundle or an explicitly supplied
location, never `Contents/`. An explicit or portable path that cannot be created, read, or written is a
visible error. VidChopper must not silently switch to the native location.

## Files and ownership

| File | Owner | Read by default | Written by CLI |
| --- | --- | ---: | ---: |
| `VidChopperCLI.ini` | CLI | When present | Yes |
| `VidChopper.ini` | GUI | Only with `--use-gui-config` | No |

`--use-gui-config` is an explicit import, not a shared-settings mode. The CLI still owns and writes only
`VidChopperCLI.ini`; it never writes `VidChopper.ini`.

The ChapterFile path is the second positional argument in the direct form, not a settings-file path:

```text
VidChopperCLI.exe <input-video> <chapters.json|chapters.yaml> [options]
VidChopperCLI.exe <input-video> --embedded [options]
VidChopperCLI.exe chop <input-video> <chapters.json|chapters.yaml> [options]
```

There is no released `--config`, `--portable`, `--ffmpeg`, or `--ffprobe` flag. Do not document or use
one until the parser and its tests establish an approved spelling.

## Precedence

Effective settings are resolved from lowest to highest precedence:

1. Built-in defaults from `ExportSettings`.
2. Optional GUI import from `VidChopper.ini`, only when `--use-gui-config` is passed.
3. CLI-owned settings from `VidChopperCLI.ini`.
4. ChapterFile `output` and `encoder` values.
5. Explicit supported CLI flags.

This keeps GUI settings out of normal CLI runs, lets a ChapterFile travel with a chapter plan, and leaves
one-run flags as the final override. Existing Windows settings remain readable, and the existing numeric
meanings of Auto, x264, and HEVC NVENC are not reset by the cross-platform resolver.

Dry runs read settings but never create or write either settings file. A normal run may create the CLI
settings file when it is missing. A settings-write failure is an error, not a reason to fall back silently.

## Persisted CLI keys

The CLI settings file persists settings the CLI owns independently of a JSON/YAML ChapterFile:

```ini
x264_crf=18
nvenc_cq=22
x264_preset=slow
nvenc_preset=p5
ffmpeg_threads=0
stop_on_first_error=false
```

The compatibility key `preset=<name>` is accepted when reading settings and sets both
`x264_preset` and `nvenc_preset`. Unknown keys are ignored for forward compatibility.

The GUI import recognizes the corresponding `[encoding]`, `[tools]`, `[output]`, and `[execution]`
sections, including encoding values, `ffmpeg`/`ffprobe` paths, output patterns, overwrite mode, manifest
options, and stop-on-first-error.

## CLI flag overrides

The parser currently supports these options and ranges:

| Flag | Value | Meaning |
| --- | --- | --- |
| `--embedded` | none | Explicitly use chapters embedded in the source. |
| `--dry-run` | none | Probe and print the plan without exporting or writing settings. |
| `--crf` | `0..51` | Override x264 CRF for this run. It does not select x264. |
| `--cq` | `0..51` | Override NVENC CQ when NVENC is selected. It does not select NVENC. |
| `--preset` | name | Apply the preset to the x264 and NVENC preset fields for this run. |
| `--threads` | `0..255` | Override FFmpeg threads; `0` leaves the FFmpeg default. |
| `--aggregate-json` | path | Write an aggregate JSON manifest. |
| `--aggregate-csv` | path | Write an aggregate CSV manifest. |
| `--stop-on-first-error` | none | Stop batch execution after the first failed item. |
| `--use-gui-config` | none | Explicitly import the GUI settings file. |
| `--version` | none | Print the installed CLI version. |
| `--help`, `-h` | none | Print the current usage text. |

## Tool paths and FFmpeg versions

The settings keys are `tools.ffmpegPath` and `tools.ffprobePath` when represented as INI sections:

```ini
[tools]
ffmpegPath=/opt/homebrew/bin/ffmpeg
ffprobePath=/opt/homebrew/bin/ffprobe
```

The foundation resolver checks an explicit configured path, then `PATH`, Homebrew locations such as
`/opt/homebrew/bin` and `/usr/local/bin`, and standard Unix locations such as `/usr/local/bin` and
`/usr/bin`. Candidates are normalized and checked once. The selected tool must exist, be executable,
launch `-version` successfully, have a parseable version, and be in the supported range `6.1` through
major `8.x`. Version `6.0` and major `9` or newer are blocked. A supported ffmpeg/ffprobe pair with
different versions continues only with a visible warning containing both paths and versions.

Missing-tool messages identify the exact path and include platform guidance: Homebrew on macOS and the
appropriate apt package on Ubuntu. VidChopper does not auto-install or download FFmpeg.

## Encoder semantics

The persisted values remain stable:

```text
Auto = 0
x264 = 1
HEVC NVENC = 2
```

Auto selection is capability-driven. A real minimal encode is the final proof; an encoder listing is only
an inexpensive prefilter. Windows and Linux use HEVC NVENC when a supported NVIDIA path passes the test,
then x264. Other systems use x264 in the `1.1.0` boundary. VideoToolbox is reserved for `1.2.0` and is
not a `1.1.0` end-user backend.

If Auto hardware capability fails, record the reason, resolve to x264 before export, and show the resolved
backend in CLI/GUI summaries and manifests where encoder data is present. If an explicit hardware backend
fails, block export with remediation and keep the stored preference unchanged. Do not retry a chapter
after it has started with a hardware backend by silently switching to x264.

`--crf` and `--cq` tune an already selected backend; they never select one. Backend-specific values must
not be interpreted as another backend's values.

## Examples

Windows packaged CLI:

```powershell
& "C:\Tools\VidChopper\VidChopperCLI.exe" `
  "C:\Media\source.mp4" `
  "C:\Media\chapters.json" `
  --use-gui-config --crf 20 --dry-run
```

Unix source-build CLI qualification:

```sh
./VidChopperCLI \
  "/Volumes/Media/source.mp4" \
  "/Volumes/Media/chapters.yaml" \
  --dry-run
```

The Unix example is a source/CI qualification command for `1.1.0`, not an end-user distribution or
support instruction.
