VidChopper for Windows 10/11 x64
================================

This portable zip contains the compiled VidChopper desktop app, the required Qt runtime files, and the Microsoft Visual C++ runtime files needed to launch it on a 64-bit Windows machine.

The archive includes both `VidChopper.exe` and `VidChopperCLI.exe`. The CLI supports chapter planning, dry runs, exports, progress reporting, and JSON/CSV manifests.
Run `VidChopperCLI.exe --version` to confirm the packaged release version.

Agent skill
-----------
New archives assembled from this source include the first-party offline skill at
`.agents\skills\vidchopper-cli\` plus `.agents\skills\vidchopper-cli.manifest.json`.
The skill keeps media local, requires a dry-run, and asks before writing or overwriting clips.

The v1.0.0 archive includes the skill and its adjacent manifest so the released CLI workflow remains
available offline. The historical v0.3.0-beta archive predates this addition and does not contain it.

Quick start
-----------
1. Extract the zip to any writable folder.
2. Run VidChopper.exe.
3. Make sure ffmpeg and ffprobe are installed, then either:
   - keep them on PATH, or
   - point VidChopper at their locations in Advanced Settings.

What is not bundled
-------------------
- ffmpeg.exe
- ffprobe.exe

VidChopper uses ffprobe to inspect source videos and ffmpeg to export chapter clips. The app can launch without them, but exporting does not work until those tools are available.

Project page
------------
https://github.com/devin-thomas/vid-chopper

Documentation rollout
---------------------
The canonical route is staged at https://vidchopper.app/docs. Until the production rollout is
live-validated, use the documentation in the source repository.
