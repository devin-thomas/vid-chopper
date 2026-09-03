VidChopperCLI 1.2.0 macOS arm64 package
=======================================

This archive contains the Qt-free VidChopperCLI executable for local use on
Apple Silicon Macs. It does not contain ffmpeg or ffprobe. Install compatible
copies separately and keep them on PATH or configure their paths through the
CLI settings contract.

The executable reports its version with:

    ./VidChopperCLI --version

Double-click `Install CLI.command` or run it from Terminal to install a copy
as `~/.local/bin/vidchopper`. It uses no sudo, preserves an existing binary as
a timestamped previous copy, and prints PATH guidance without editing shell
startup files.

The package is a local development candidate, not a notarized or published
release. Review THIRD_PARTY_NOTICES.txt and LICENSE before sharing it.
