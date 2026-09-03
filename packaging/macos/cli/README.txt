VidChopperCLI 1.2.0 macOS arm64 package
=======================================

This archive contains the Qt-free VidChopperCLI executable for Apple Silicon
Macs running macOS 15 or newer. It does not contain ffmpeg or ffprobe. Install
compatible copies separately and keep them on PATH or configure their paths
through the CLI settings contract.

The executable reports its version with:

    ./VidChopperCLI --version

Double-click `Install CLI.command` or run it from Terminal to install a copy
as `~/.local/bin/vidchopper`. It uses no sudo, preserves an existing binary as
a timestamped previous copy, and prints PATH guidance without editing shell
startup files.

The executable is ad-hoc signed, not Developer ID signed or notarized. Review
THIRD_PARTY_NOTICES.txt and LICENSE before using or redistributing it.
