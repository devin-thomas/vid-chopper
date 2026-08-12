#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="1.2.0"
ARCHITECTURE="arm64"
APP_INPUT="${VIDCHOPPER_APP:-$ROOT_DIR/build/macos-gui-release/VidChopper.app}"
CLI_INPUT=""
OUTPUT_DIR="${VIDCHOPPER_OUTPUT_DIR:-$ROOT_DIR/dist/macos}"
DO_SIGN=0

usage() {
    cat <<'USAGE'
Usage: tools/package-macos-app.sh [options]

Create a local arm64 VidChopper disk image. This script does not notarize,
publish, alter Gatekeeper settings, or bundle ffmpeg/ffprobe.

Options:
  --app PATH           Use PATH as the built VidChopper.app.
  --cli PATH           Add a CLI executable, directory, or .tar.gz archive.
  --sign               Apply an ad-hoc signature to the app bundle.
  --output-dir PATH    Write the DMG and checksum under PATH.
  --help               Show this help.
USAGE
}

while (($# > 0)); do
    case "$1" in
        --app|--cli|--output-dir)
            if (($# < 2)); then
                echo "$1 requires a path" >&2
                exit 2
            fi
            case "$1" in
                --app) APP_INPUT="$2" ;;
                --cli) CLI_INPUT="$2" ;;
                --output-dir) OUTPUT_DIR="$2" ;;
            esac
            shift
            ;;
        --sign)
            DO_SIGN=1
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

for required_command in codesign ditto file hdiutil otool shasum tar; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "Missing required command: $required_command" >&2
        exit 1
    fi
done

if [[ ! -d "$APP_INPUT" || ! -x "$APP_INPUT/Contents/MacOS/VidChopper" ]]; then
    echo "VidChopper.app not found or incomplete: $APP_INPUT" >&2
    exit 1
fi
if [[ "$(uname -m)" != "$ARCHITECTURE" ]]; then
    echo "The local app package requires an arm64 Apple Silicon host." >&2
    exit 1
fi
if ! file "$APP_INPUT/Contents/MacOS/VidChopper" | rg -q 'arm64'; then
    echo "Application executable is not arm64: $APP_INPUT" >&2
    exit 1
fi

STAGE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/vidchopper-app-package.XXXXXX")"
IMAGE_ROOT="$STAGE_DIR/VidChopper-1.2.0"
PACKAGE_APP="$IMAGE_ROOT/VidChopper.app"
mkdir -p "$IMAGE_ROOT"
ditto "$APP_INPUT" "$PACKAGE_APP"

if command -v macdeployqt >/dev/null 2>&1 \
    && [[ ! -d "$PACKAGE_APP/Contents/Frameworks/QtCore.framework" ]]; then
    macdeployqt "$PACKAGE_APP" -always-overwrite
fi

if ((DO_SIGN == 1)); then
    codesign --force --deep --sign - "$PACKAGE_APP"
    codesign --verify --deep --strict "$PACKAGE_APP"
fi

if otool -L "$PACKAGE_APP/Contents/MacOS/VidChopper" | rg -q '/Users/research/homebrew|/opt/homebrew|/usr/local/opt'; then
    echo "Packaged app still links to a developer-local library path." >&2
    exit 1
fi

ln -s /Applications "$IMAGE_ROOT/Applications"
cp "$ROOT_DIR/LICENSE" "$IMAGE_ROOT/LICENSE"
cp "$ROOT_DIR/packaging/macos/app/README.txt" "$IMAGE_ROOT/README.txt"
cp "$ROOT_DIR/packaging/macos/app/THIRD_PARTY_NOTICES.txt" "$IMAGE_ROOT/THIRD_PARTY_NOTICES.txt"
cp "$ROOT_DIR/packaging/macos/app/TRUST_GUIDANCE.txt" "$IMAGE_ROOT/TRUST_GUIDANCE.txt"

if [[ -n "$CLI_INPUT" ]]; then
    CLI_DEST="$IMAGE_ROOT/VidChopperCLI"
    case "$CLI_INPUT" in
        *.tar.gz|*.tgz)
            tar -xzf "$CLI_INPUT" -C "$IMAGE_ROOT"
            ;;
        *)
            if [[ -d "$CLI_INPUT" ]]; then
                ditto "$CLI_INPUT" "$CLI_DEST"
            elif [[ -x "$CLI_INPUT" ]]; then
                mkdir -p "$CLI_DEST"
                ditto "$CLI_INPUT" "$CLI_DEST/VidChopperCLI"
                cp "$ROOT_DIR/packaging/macos/cli/THIRD_PARTY_NOTICES.txt" "$CLI_DEST/THIRD_PARTY_NOTICES.txt"
                cp "$ROOT_DIR/packaging/macos/cli/Install CLI.command" "$CLI_DEST/Install CLI.command"
                chmod +x "$CLI_DEST/Install CLI.command"
            else
                echo "CLI input is not an executable, directory, or tar archive: $CLI_INPUT" >&2
                exit 1
            fi
        esac
fi

mkdir -p "$OUTPUT_DIR"
DMG_PATH="$OUTPUT_DIR/VidChopper-$VERSION-macos-$ARCHITECTURE.dmg"
CHECKSUM_PATH="$DMG_PATH.sha256"
if [[ -e "$DMG_PATH" ]]; then
    mv "$DMG_PATH" "$DMG_PATH.previous.$(date +%Y%m%d-%H%M%S)"
fi
if [[ -e "$CHECKSUM_PATH" ]]; then
    mv "$CHECKSUM_PATH" "$CHECKSUM_PATH.previous.$(date +%Y%m%d-%H%M%S)"
fi
hdiutil create -volname "VidChopper 1.2.0" -srcfolder "$IMAGE_ROOT" -format UDZO -ov "$DMG_PATH"
shasum -a 256 "$DMG_PATH" > "$CHECKSUM_PATH"

echo "Created $DMG_PATH"
echo "Checksum: $CHECKSUM_PATH"
if ((DO_SIGN == 1)); then
    echo "The app bundle is ad-hoc signed; it is not notarized or published."
else
    echo "The app bundle is unsigned; use --sign for an ad-hoc local signature."
fi
