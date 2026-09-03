#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
USER_HOME="${HOME:?HOME must be set}"
BUILD_DIR="${VIDCHOPPER_BUILD_DIR:-$ROOT_DIR/build/macos-gui-release}"
INSTALL_DIR="${VIDCHOPPER_INSTALL_DIR:-$USER_HOME/Applications}"
export VCPKG_ROOT="${VCPKG_ROOT:-$ROOT_DIR/.vcpkg}"
BUILD_JOBS="${VIDCHOPPER_BUILD_JOBS:-4}"
MODE="run"
NO_BUILD=0
NO_LAUNCH=0

usage() {
    cat <<'USAGE'
Usage: script/build_and_run.sh [options]

Build, deploy, install, and launch the local VidChopper macOS candidate.

Options:
  --debug             Launch the installed app under lldb.
  --logs              Launch and stream VidChopper process logs.
  --telemetry         Launch and stream VidChopper subsystem logs.
  --verify            Launch and verify that the app process starts.
  --no-build          Reuse the existing CMake build directory.
  --no-launch         Build and install without opening the app.
  --install-dir PATH  Install into PATH instead of ~/Applications.
  --help              Show this help.
USAGE
}

while (($# > 0)); do
    case "$1" in
        --debug|debug)
            MODE="debug"
            ;;
        --logs|logs)
            MODE="logs"
            ;;
        --telemetry|telemetry)
            MODE="telemetry"
            ;;
        --verify|verify)
            MODE="verify"
            ;;
        --no-build)
            NO_BUILD=1
            ;;
        --no-launch)
            NO_LAUNCH=1
            ;;
        --install-dir)
            if (($# < 2)); then
                echo "--install-dir requires a path" >&2
                exit 2
            fi
            INSTALL_DIR="$2"
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

if [[ "$(uname -m)" != "arm64" ]]; then
    echo "VidChopper 1.2.0 local macOS builds require an arm64 Apple Silicon host." >&2
    exit 1
fi

for required_command in codesign cmake ditto file find open pgrep plutil otool; do
    if ! command -v "$required_command" >/dev/null 2>&1; then
        echo "Missing required command: $required_command" >&2
        exit 1
    fi
done

if [[ ! -x "$VCPKG_ROOT/vcpkg" ]]; then
    echo "Missing vcpkg at $VCPKG_ROOT. Bootstrap it with:" >&2
    echo "  git clone https://github.com/microsoft/vcpkg.git .vcpkg" >&2
    echo "  ./.vcpkg/bootstrap-vcpkg.sh -disableMetrics" >&2
    exit 1
fi

QT6_DIR="${Qt6_DIR:-}"
if [[ -z "$QT6_DIR" || ! -f "$QT6_DIR/Qt6Config.cmake" ]]; then
    for qt_candidate in \
        "/Users/research/homebrew/lib/cmake/Qt6" \
        "/opt/homebrew/opt/qt/lib/cmake/Qt6" \
        "/opt/homebrew/opt/qtbase/lib/cmake/Qt6" \
        "/usr/local/opt/qt/lib/cmake/Qt6" \
        "/usr/local/opt/qtbase/lib/cmake/Qt6"; do
        if [[ -f "$qt_candidate/Qt6Config.cmake" ]]; then
            QT6_DIR="$qt_candidate"
            break
        fi
    done
fi

if [[ -z "$QT6_DIR" || ! -f "$QT6_DIR/Qt6Config.cmake" ]]; then
    echo "Could not find Qt6Config.cmake. Set Qt6_DIR to the Qt 6 CMake directory." >&2
    exit 1
fi

if ((NO_BUILD == 0)); then
    cmake --preset macos-gui-release -DQt6_DIR="$QT6_DIR"
    cmake --build --preset macos-gui-release --parallel "$BUILD_JOBS"
fi

if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
    echo "Build directory is not configured: $BUILD_DIR" >&2
    exit 1
fi

STAGE_DIR="$(mktemp -d "${TMPDIR:-/tmp}/vidchopper-install.XXXXXX")"
cmake --install "$BUILD_DIR" --prefix "$STAGE_DIR"

APP_SOURCE="$STAGE_DIR/VidChopper.app"
if [[ ! -d "$APP_SOURCE" ]]; then
    APP_SOURCE="$(find "$STAGE_DIR" -maxdepth 3 -type d -name 'VidChopper.app' -print -quit)"
fi
if [[ -z "$APP_SOURCE" || ! -d "$APP_SOURCE" ]]; then
    echo "CMake install did not produce VidChopper.app under $STAGE_DIR" >&2
    exit 1
fi

APP_INSTALL="$INSTALL_DIR/VidChopper.app"
mkdir -p "$INSTALL_DIR"
previous_app=""
if [[ -e "$APP_INSTALL" || -L "$APP_INSTALL" ]]; then
    previous_app="$APP_INSTALL.previous.$(date +%Y%m%d-%H%M%S)"
    mv "$APP_INSTALL" "$previous_app"
fi

if ! ditto "$APP_SOURCE" "$APP_INSTALL"; then
    if [[ -n "$previous_app" && ! -e "$APP_INSTALL" ]]; then
        mv "$previous_app" "$APP_INSTALL"
    fi
    exit 1
fi

# The Qt deployment helper can rewrite already signed frameworks while copying
# them. Re-sign the final local bundle so the installed candidate is internally
# consistent without claiming notarization or developer identity.
codesign --force --deep --sign - "$APP_INSTALL"
codesign --verify --deep --strict "$APP_INSTALL"

APP_BINARY="$APP_INSTALL/Contents/MacOS/VidChopper"
APP_PLIST="$APP_INSTALL/Contents/Info.plist"
if [[ ! -x "$APP_BINARY" || ! -f "$APP_PLIST" ]]; then
    echo "Installed app is missing its executable or Info.plist: $APP_INSTALL" >&2
    exit 1
fi

if [[ "$(plutil -extract CFBundleShortVersionString raw -o - "$APP_PLIST")" != "1.2.0" ]]; then
    echo "Installed app does not report version 1.2.0" >&2
    exit 1
fi
if ! file "$APP_BINARY" | rg -q 'arm64'; then
    echo "Installed app executable is not arm64: $APP_BINARY" >&2
    exit 1
fi
if otool -L "$APP_BINARY" | rg -q '/Users/research/homebrew|/opt/homebrew|/usr/local/opt'; then
    echo "Installed app still links to a developer-local library path" >&2
    exit 1
fi

if ((NO_LAUNCH == 1)); then
    echo "Installed local VidChopper 1.2.0 at $APP_INSTALL"
    exit 0
fi

launch_app() {
    pkill -x VidChopper >/dev/null 2>&1 || true
    /usr/bin/open -n "$APP_INSTALL"
}

case "$MODE" in
    run)
        launch_app
        echo "Launched local VidChopper 1.2.0 from $APP_INSTALL"
        ;;
    debug)
        lldb -- "$APP_BINARY"
        ;;
    logs)
        launch_app
        exec /usr/bin/log stream --info --style compact --predicate 'process == "VidChopper"'
        ;;
    telemetry)
        launch_app
        exec /usr/bin/log stream --info --style compact --predicate 'subsystem == "com.vidchopper.VidChopper"'
        ;;
    verify)
        launch_app
        for attempt in {1..10}; do
            if pgrep -x VidChopper >/dev/null 2>&1; then
                echo "VidChopper 1.2.0 started successfully from $APP_INSTALL"
                exit 0
            fi
            sleep 1
        done
        echo "VidChopper did not remain running after launch" >&2
        exit 1
        ;;
    *)
        echo "Unsupported mode: $MODE" >&2
        exit 2
        ;;
esac
