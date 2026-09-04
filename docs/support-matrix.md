# 1.2.0 Support Matrix

`1.2.0` is the first Apple Silicon macOS end-user release and remains a cumulative Windows release. The public support change takes effect only after the exact candidates pass the automated and physical gates in the [1.2.0 evidence record](1.2.0-release-evidence.md). Until that publication occurs, 1.1.0 remains the current stable download.

## End-user boundary

| Platform and architecture | Qualification evidence | Public binary in 1.2.0 | End-user support in 1.2.0 |
| --- | --- | --- | --- |
| Windows 10/11 x64 | Core, CLI, GUI, fast/slow/Qt tests, package regression, and exact clean-archive smoke | Windows portable ZIP and adjacent checksum | Yes |
| macOS 15+ arm64 | Hosted builds and exact-candidate smoke plus full journeys on a physical M4 Pro | DMG, standalone CLI archive, and adjacent checksums | Yes |
| Intel macOS | None | No | No |
| Ubuntu 24.04/26.04 x86-64 | Native core/CLI tests and GUI compile/launch smoke | No | No; planned for 1.3.0 |
| Linux arm64 | Source compatibility only; separate 1.3.0 qualification is pending | No | No; planned for 1.3.0 |

Linux build and CI results prove source portability only. They are not release assets, installation packages, or an end-user support claim.

## macOS package and trust contract

- The DMG contains the arm64 app, an Applications link, notices, trust guidance, and the standalone CLI directory.
- The CLI archive installs to `~/.local/bin/vidchopper` without `sudo` and does not edit shell startup files.
- The app and CLI are ad-hoc signed. They are not Developer ID signed or notarized.
- Browser-downloaded copies can require the documented per-app Finder/System Settings approval flow. VidChopper never disables Gatekeeper globally or removes quarantine metadata.
- FFmpeg and ffprobe remain external and are supported from 6.1 through major 9.x for the 1.2.0 boundary.
- Auto uses HEVC VideoToolbox on supported Apple Silicon after a real capability encode and otherwise resolves to x264 before export.

## Qualification lanes

Publication requires one source commit and exact hashes across every lane:

- Windows x64 Release tier plus a second-runner archive smoke.
- macOS candidate build plus a second-runner checksum, signature, dependency, relocated-launch, CLI, x264, manifest, and VideoToolbox capability smoke.
- Physical M4 Pro journeys for installation/relaunch, probing, embedded/default chapter editing, x264 and VideoToolbox exports, cancellation without orphan processes, manifests, output-folder opening, settings/error paths, and direct/installed CLI use.
- A browser-downloaded quarantine and per-app first-launch approval journey on at least one physical Mac.
- A protected promotion that downloads the retained candidate set, checks all evidence and hashes, publishes six unchanged assets, and compares every remote download byte-for-byte.

Any failed or missing assigned check blocks publication. Building a replacement after qualification creates a new candidate and requires requalification.
