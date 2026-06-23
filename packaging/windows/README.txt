VidChopper for Windows 10/11 x64
================================

This portable zip contains the compiled VidChopper desktop app, the required Qt runtime files, and the Microsoft Visual C++ runtime files needed to launch it on a 64-bit Windows machine.

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
