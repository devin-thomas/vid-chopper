VidChopper 1.2.0 for macOS
==========================

This folder contains a locally assembled VidChopper application image. It is
intended for local testing and controlled hand-off, not for an automatic
update channel or a hosted release.

Install
-------
1. Drag VidChopper.app onto the Applications icon in the disk image.
2. Eject the disk image.
3. Launch VidChopper from Applications.

The disk image contains an Applications alias for the normal Finder install
flow. It does not change Gatekeeper settings, remove quarantine metadata, or
install anything outside the application and the files shown in the image.

Video tools
-----------
ffmpeg and ffprobe are intentionally not bundled. VidChopper starts without
them, but probing and exporting require compatible local copies. Configure
their paths in VidChopper's Advanced Settings, or make them available on the
user's PATH. Review and accept the license terms for any video tools installed
separately.

Optional CLI
------------
When a CLI input is supplied to the local packaging script, the disk image
also contains a VidChopperCLI directory. The CLI and its non-FFmpeg runtime
files remain local to that directory.

Local trust
-----------
Read TRUST_GUIDANCE.txt before opening a locally built package. A package
created with the script's --sign option is ad-hoc signed, not notarized, and
may cause macOS to display an unidentified-developer warning. Verify the
checksum supplied beside the disk image and only approve a package whose
provenance you understand.

See THIRD_PARTY_NOTICES.txt and LICENSE for licensing information.
