VidChopper 1.2.0 for macOS
==========================

This disk image contains the Apple Silicon VidChopper application and the
standalone command-line package for the 1.2.0 release.

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
The VidChopperCLI directory contains the standalone command-line archive
contents and a no-sudo user-local installer.

Trust and first launch
----------------------
Read TRUST_GUIDANCE.txt before opening the package. The release is ad-hoc
signed, not Developer ID signed or notarized, and may cause macOS to display
an unidentified-developer warning. Verify the checksum supplied beside the
disk image and only use the per-app Finder/System Settings approval flow for
a package whose provenance you understand. Never disable Gatekeeper globally.

See THIRD_PARTY_NOTICES.txt and LICENSE for licensing information.
