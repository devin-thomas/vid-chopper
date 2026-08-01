# VidChopper CLI chapter config schema

The CLI chapter config is the explicit chapter source for `VidChopperCLI.exe`. JSON and YAML are interchangeable: the same object shape, field names, timestamp policy, and validation rules apply to both formats. The Qt-free loader lives in `src/cli/chapter_config.cpp` and uses `nlohmann-json` and `yaml-cpp` from the pinned vcpkg manifest.

## Top-level shape

```yaml
version: 1
output:
  folder: "%source%_chapters"
  namingPattern: "%index% - %name%"
encoder:
  crf: 20
  cq: 23
  preset: slow
  threads: 4
chapters:
  - name: Opening
    start: "00:00:00.000"
    end: "00:01:15.500"
```

Only `chapters` is required. `version` is optional for now, but examples include `version: 1` so future migrations have a stable place to branch from.

## Fields

| Field | Type | Required | Meaning |
|---|---:|---:|---|
| `$schema` | string | No | Optional schema hint for editors and validators. |
| `version` | integer | No | Schema version. Currently only `1` is valid. |
| `output.folder` | string | No | Overrides the output folder pattern for this config. |
| `output.namingPattern` | string | No | Overrides the output file naming pattern for this config. |
| `encoder.crf` | integer `0..51` | No | x264 CRF override for this config. |
| `encoder.cq` | integer `0..51` | No | NVENC CQ override for this config. |
| `encoder.preset` | string | No | Encoder preset override for this config. |
| `encoder.threads` | integer `0..255` | No | ffmpeg thread-count override for this config. `0` means ffmpeg default. |
| `chapters[].name` | string | Yes | Human-readable chapter name. |
| `chapters[].start` | timestamp string or non-negative integer milliseconds | Yes | Inclusive chapter start time. |
| `chapters[].end` | timestamp string or non-negative integer milliseconds | Yes | Exclusive chapter end time. |
| `chapters[].outputName` | string | No | Optional per-chapter output-name override. |

Unknown fields are invalid. The loader should reject them with a human-readable validation error instead of silently ignoring misspelled config.

## Timestamp policy

Chapter timestamps use the existing core millisecond timecode style, not Qt types and not locale-specific dates.

Accepted forms:

```text
MM:SS
MM:SS.m
MM:SS.mm
MM:SS.mmm
HH:MM:SS
HH:MM:SS.mmm
```

Non-negative integer millisecond values are also accepted, which is useful for generated configs:

```json
{ "start": 60000, "end": "00:02:30.000" }
```

Rules:

- Hours are optional and may be more than two digits.
- Minutes and seconds must be `0..59`.
- Fractional seconds may contain one to three digits and are normalized to milliseconds.
- `start` must be less than `end`.
- Chapters must be in non-overlapping timeline order.
- Empty names are invalid.

Use quoted strings in YAML examples so values like `00:01:15.500` are not interpreted as YAML scalars with special behavior.

## Settings precedence

Config-local fields sit between loaded settings and explicit CLI flags:

1. Built-in CLI defaults.
2. Optional GUI import from `VidChopper.ini`, only when `--use-gui-config` is passed.
3. CLI-owned settings from `VidChopperCLI.ini`.
4. Config-local fields from this JSON/YAML file.
5. Explicit CLI flags.

This means config files can travel with a video/chapter plan, but command-line flags remain the final override for one-off runs.

## Validation errors

Bad configs should fail before export starts. Error messages should name the file and the config path that failed.

Recommended error examples:

```text
chapters: at least one chapter is required
chapters[0].name: chapter name is required
chapters[0].start: expected MM:SS(.mmm) or HH:MM:SS(.mmm)
chapters[1].end: must be greater than start
encoder.crf: must be an integer from 0 to 51
output.namingPattern: unknown placeholder %bad%
```

The schema file catches structural issues such as missing `chapters`, unknown fields, and numeric ranges. Timeline checks such as `end > start` and overlap detection are domain validation rules for the loader. JSON parser failures include a byte location; YAML parser failures include line and column; both forms include the source path.

## Files added for this schema

- `docs/schemas/chapter-config.schema.json`
- `examples/chapter-config.json`
- `examples/chapter-config.yaml`
- `examples/invalid/chapter-config-missing-chapters.json`
- `examples/invalid/chapter-config-invalid-order.yaml`
- `src/cli/chapter_config.hpp` and `src/cli/chapter_config.cpp`
- `tests/test_chapter_config.cpp`
- `vcpkg.json`

## ChapterBuilder compatibility fixture

`tests/fixtures/chapterbuilder/tns-2xko-36-chapters.json` is byte-identical to the
fixture exported by ChapterBuilder. It uses the public TNS 2XKO #36 VOD timestamps
and Top 8 bracket while keeping tournament-specific text inside the game-neutral
`name` and `outputName` fields:

- VOD: https://youtu.be/un-_oJrC-RI
- Event: https://www.start.gg/tournament/tns-2xko-36/events

The fast loader test proves that VidChopper accepts the export without edits. To
exercise the complete contract against a local source video, build the CLI and run:

```powershell
.\build\core-release\Release\VidChopperCLI.exe `
  <source-video> `
  .\tests\fixtures\chapterbuilder\tns-2xko-36-chapters.json `
  --dry-run
```

The repository intentionally does not include the downloaded VOD or generated
clips. Real-media validation artifacts remain local and ignored by Git.
