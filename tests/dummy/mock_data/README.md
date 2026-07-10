# Dummy CLI Mock Data

This directory contains small text fixtures for the first CLI implementation phases.

The files are intentionally lightweight and are not real media. They give parser, config, batch-pairing, and settings tests stable paths and payloads before ffmpeg-backed integration fixtures are added.

## Files

- `mock_stream.mkv` - placeholder source video path fixture.
- `mock_stream.json` - JSON chapter config fixture.
- `mock_stream.yaml` - YAML chapter config fixture with the same logical chapters.
- `duplicate_stem.json` and `duplicate_stem.yaml` - future N:N duplicate-config-stem rejection fixtures.
- `mock_ffprobe_embedded_chapters.json` - future no-Qt ffprobe parser fixture.
- `VidChopperCLI.ini` - CLI settings fixture.
- `VidChopper.ini` - GUI settings fixture used to verify explicit-only GUI config import.
