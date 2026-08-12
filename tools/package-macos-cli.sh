#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
USER_HOME="${HOME:?HOME must be set}"
VERSION="1.2.0"
ARCHITECTURE="arm64"
CLI_BINARY="${VIDCHOPPER_CLI:-$ROOT_DIR/build/macos-gui-release/VidChopperCLI}"
OUTPUT_DIR="${VIDCHOPPER_OUTPUT_DIR:-$ROOT_DIR/dist/macos}"
INSTALL_DIR="${VIDCHOPPER_CLI_INSTALL_DIR:-$USER_HOME/.local/bin}"
DO_BUILD=0
DO_INSTALL=0

usage() {
    cat <<'USAGE'
Usage: tools/package-macos-cli.sh [options]

Package the locally built arm64 VidChopperCLI without ffmpeg or ffprobe.

Options:
  --build             Build the macOS GUI + CLI preset first.
  --install           Install a copy as ~/.local/bin/vidchopper.
  --output-dir PATH   Write the archive and checksum under PATH.
  --cli PATH          Use PATH as the CLI executable.
  --help              Show this help.
USAGE
}

while (($# > 0)); do
    case "$1" in
        --build)
            DO_BUILD=1
            ;;
        --install)
            DO_INSTALL=1
            ;;
        --output-dir|--cli)
            if (($# < 2)); then
                echo "$1 requires a path" >&2
                exit 2
            fi
            if [[ "$1" == "--output-dir" ]]; then
                OUTPUT_DIR="$2"
            else
                CLI_BINARY="$2"
            fi
            shift
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

if ((DO_BUILD == 1)); then
    cmake --build --preset macos-gui-release --parallel "${VIDCHOPPER_BUILD_JOBS:-4}"
fi

if [[ ! -x "$CLI_BINARY" ]]; then
    echo "CLI executable not found or not executable: $CLI_BINARY" >&2
    echo "Build it with script/build_and_run.sh or pass --cli PATH." >&2
    exit 1
fi
if [[ "$(uname -m)" != "$ARCHITECTURE" ]]; then
    echo "The local CLI package requires an arm64 Apple Silicon host." >&2
    exit 1
fi
if ! command -v codesign >/dev/null 2>&1; then
    echo "Missing required command: codesign" >&2
    exit 1
fi
if ! file "$CLI_BINARY" | rg -q 'arm64'; then
    echo "CLI executable is not arm64: $CLI_BINARY" >&2
    exit 1
fi
if otool -L "$CLI_BINARY" | rg -qi 'ffmpeg|ffprobe'; then
    echo "CLI unexpectedly links to ffmpeg or ffprobe; those tools must remain external." >&2
    exit 1
fi

CLI_VERSION_OUTPUT="$($CLI_BINARY --version)"
if [[ "$CLI_VERSION_OUTPUT" != *"$VERSION"* ]]; then
    echo "CLI did not report version $VERSION: $CLI_VERSION_OUTPUT" >&2
    exit 1
fi

mkdir -p "$OUTPUT_DIR"
STAGE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/vidchopper-cli-package.XXXXXX")"
PACKAGE_DIR="$STAGE_DIR/VidChopperCLI"
mkdir -p "$PACKAGE_DIR"
ditto "$CLI_BINARY" "$PACKAGE_DIR/VidChopperCLI"
chmod +x "$PACKAGE_DIR/VidChopperCLI"
codesign --force --sign - "$PACKAGE_DIR/VidChopperCLI"
codesign --verify --strict "$PACKAGE_DIR/VidChopperCLI"
cp "$ROOT_DIR/LICENSE" "$PACKAGE_DIR/LICENSE"
cp "$ROOT_DIR/packaging/macos/cli/THIRD_PARTY_NOTICES.txt" "$PACKAGE_DIR/THIRD_PARTY_NOTICES.txt"
cp "$ROOT_DIR/packaging/macos/cli/Install CLI.command" "$PACKAGE_DIR/Install CLI.command"
chmod +x "$PACKAGE_DIR/Install CLI.command"
printf '%s\n' "$CLI_VERSION_OUTPUT" > "$PACKAGE_DIR/VERSION.txt"

ARCHIVE_PATH="$OUTPUT_DIR/VidChopper-$VERSION-macos-$ARCHITECTURE-cli.tar.gz"
CHECKSUM_PATH="$ARCHIVE_PATH.sha256"
if [[ -e "$ARCHIVE_PATH" ]]; then
    mv "$ARCHIVE_PATH" "$ARCHIVE_PATH.previous.$(date +%Y%m%d-%H%M%S)"
fi
if [[ -e "$CHECKSUM_PATH" ]]; then
    mv "$CHECKSUM_PATH" "$CHECKSUM_PATH.previous.$(date +%Y%m%d-%H%M%S)"
fi
tar -C "$STAGE_DIR" -czf "$ARCHIVE_PATH" VidChopperCLI
shasum -a 256 "$ARCHIVE_PATH" > "$CHECKSUM_PATH"

if ((DO_INSTALL == 1)); then
    mkdir -p "$INSTALL_DIR"
    install_stage="$(mktemp -d "$INSTALL_DIR/.vidchopper-install.XXXXXX")"
    ditto "$PACKAGE_DIR/VidChopperCLI" "$install_stage/vidchopper"
    chmod +x "$install_stage/vidchopper"
    installed_binary="$INSTALL_DIR/vidchopper"
    if [[ -e "$installed_binary" || -L "$installed_binary" ]]; then
        mv "$installed_binary" "$installed_binary.previous.$(date +%Y%m%d-%H%M%S)"
    fi
    mv "$install_stage/vidchopper" "$installed_binary"
    rmdir "$install_stage"
    echo "Installed CLI at $installed_binary"
fi

echo "Created $ARCHIVE_PATH"
echo "Checksum: $CHECKSUM_PATH"
