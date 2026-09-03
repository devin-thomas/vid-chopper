#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
USER_HOME="${HOME:?HOME must be set}"
INSTALL_DIR="${VIDCHOPPER_CLI_INSTALL_DIR:-$USER_HOME/.local/bin}"
SOURCE_BINARY="$SCRIPT_DIR/VidChopperCLI"
TARGET_BINARY="$INSTALL_DIR/vidchopper"

if [[ ! -x "$SOURCE_BINARY" ]]; then
    echo "Could not find an executable VidChopperCLI beside this installer." >&2
    exit 1
fi

mkdir -p "$INSTALL_DIR"
INSTALL_STAGE="$(mktemp -d "$INSTALL_DIR/.vidchopper-install.XXXXXX")"
ditto "$SOURCE_BINARY" "$INSTALL_STAGE/vidchopper"
chmod +x "$INSTALL_STAGE/vidchopper"

if [[ -e "$TARGET_BINARY" || -L "$TARGET_BINARY" ]]; then
    mv "$TARGET_BINARY" "$TARGET_BINARY.previous.$(date +%Y%m%d-%H%M%S)"
fi
mv "$INSTALL_STAGE/vidchopper" "$TARGET_BINARY"
rmdir "$INSTALL_STAGE"

echo "Installed VidChopperCLI 1.2.0 at $TARGET_BINARY"
case ":${PATH:-}:" in
    *":$INSTALL_DIR:"*)
        echo "Run: vidchopper --version"
        ;;
    *)
        echo "Add this directory to PATH, then run: vidchopper --version"
        echo "  $INSTALL_DIR"
        ;;
esac
