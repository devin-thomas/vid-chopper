# VidChopper CLI settings

The command-line app has its own settings file so CLI preferences do not mutate or depend on GUI preferences.

## Files

| File | Owner | Read by default | Written by CLI |
|---|---|---:|---:|
| `VidChopperCLI.ini` | CLI | Yes | Yes |
| `VidChopper.ini` | GUI | No | No |

`VidChopperCLI.ini` lives beside `VidChopperCLI.exe`. `VidChopper.ini` lives beside the app as the GUI settings file. The CLI resolves both paths so it can report what it is doing, but it only reads the GUI file when the user passes `--use-gui-config`.

## Precedence

Effective CLI settings are resolved in this order:

1. Built-in CLI defaults from `ExportSettings`.
2. Optional GUI import from `VidChopper.ini`, only when `--use-gui-config` is passed.
3. CLI-owned settings from `VidChopperCLI.ini`.
4. Explicit chapter config fields from the JSON/YAML ChapterFile loader.
5. Explicit CLI flags.

This means GUI settings are never read implicitly, CLI-owned settings win over optional GUI import, and CLI flags always win over loaded settings.

## Persisted CLI keys

The CLI settings file persists the settings the CLI owns independently of a JSON/YAML ChapterFile:

```ini
x264_crf=18
nvenc_cq=22
x264_preset=slow
nvenc_preset=p5
ffmpeg_threads=0
stop_on_first_error=true
```

The compatibility key `preset=<name>` is also accepted when reading settings. It sets both `x264_preset` and `nvenc_preset`, matching the current `--preset` CLI flag behavior.

Unknown keys are ignored so future versions can add settings without breaking older builds.

## GUI import policy

`--use-gui-config` is an explicit import, not a shared settings mode. The CLI still creates and owns `VidChopperCLI.ini`, and it never writes `VidChopper.ini`.

When GUI import is enabled, GUI values fill in settings that the CLI INI does not override. This allows a one-time convenience import while keeping CLI preferences separate.
