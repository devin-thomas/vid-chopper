#!/usr/bin/env bash
set -euo pipefail

VERSION=""
DMG_PATH=""
CLI_ARCHIVE_PATH=""
EVIDENCE_PATH=""

usage() {
    cat <<'USAGE'
Usage: tools/verify-macos-candidate.sh --version VERSION --dmg PATH --cli-archive PATH --evidence PATH

Verify exact VidChopper macOS candidate bytes on an Apple Silicon Mac and write
machine-readable evidence. The candidate files are never modified.
USAGE
}

while (($# > 0)); do
    case "$1" in
        --version|--dmg|--cli-archive|--evidence)
            if (($# < 2)); then
                echo "$1 requires a value" >&2
                exit 2
            fi
            case "$1" in
                --version) VERSION="$2" ;;
                --dmg) DMG_PATH="$2" ;;
                --cli-archive) CLI_ARCHIVE_PATH="$2" ;;
                --evidence) EVIDENCE_PATH="$2" ;;
            esac
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

if [[ -z "$VERSION" || -z "$DMG_PATH" || -z "$CLI_ARCHIVE_PATH" || -z "$EVIDENCE_PATH" ]]; then
    usage >&2
    exit 2
fi

for command_name in codesign ditto ffmpeg ffprobe file hdiutil jq lipo otool shasum tar; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        echo "Missing required command: $command_name" >&2
        exit 1
    fi
done

if [[ "$(uname -s)" != "Darwin" || "$(uname -m)" != "arm64" ]]; then
    echo "macOS candidate verification requires an Apple Silicon Mac." >&2
    exit 1
fi

for candidate_path in "$DMG_PATH" "$CLI_ARCHIVE_PATH"; do
    if [[ ! -f "$candidate_path" ]]; then
        echo "Candidate file not found: $candidate_path" >&2
        exit 1
    fi
    if [[ ! -f "${candidate_path}.sha256" ]]; then
        echo "Candidate checksum not found: ${candidate_path}.sha256" >&2
        exit 1
    fi
done

checksum_hash() {
    awk 'NR == 1 { print $1 }' "$1"
}

verify_checksum_pair() {
    local candidate_path="$1"
    local checksum_path="${candidate_path}.sha256"
    local expected_hash
    local recorded_name
    local actual_hash
    expected_hash="$(checksum_hash "$checksum_path")"
    recorded_name="$(awk 'NR == 1 { print $2 }' "$checksum_path")"
    recorded_name="${recorded_name#\*}"
    actual_hash="$(shasum -a 256 "$candidate_path" | awk '{ print $1 }')"
    if [[ "$expected_hash" != "$actual_hash" ]]; then
        echo "Checksum mismatch for $candidate_path" >&2
        exit 1
    fi
    if [[ "$(basename "$recorded_name")" != "$(basename "$candidate_path")" ]]; then
        echo "Checksum filename mismatch for $candidate_path: $recorded_name" >&2
        exit 1
    fi
}

verify_checksum_pair "$DMG_PATH"
verify_checksum_pair "$CLI_ARCHIVE_PATH"
ORIGINAL_DMG_HASH="$(shasum -a 256 "$DMG_PATH" | awk '{ print $1 }')"
ORIGINAL_CLI_HASH="$(shasum -a 256 "$CLI_ARCHIVE_PATH" | awk '{ print $1 }')"

WORK_DIR="$(mktemp -d "${TMPDIR:-/tmp}/vidchopper-macos-candidate.XXXXXX")"
MOUNT_DIR="$WORK_DIR/mounted"
GUI_PID=""
MOUNTED=0

cleanup() {
    if [[ -n "$GUI_PID" ]] && kill -0 "$GUI_PID" 2>/dev/null; then
        kill "$GUI_PID" 2>/dev/null || true
        wait "$GUI_PID" 2>/dev/null || true
    fi
    if ((MOUNTED == 1)); then
        hdiutil detach "$MOUNT_DIR" -quiet >/dev/null 2>&1 || true
    fi
    rm -rf "$WORK_DIR"
}
trap cleanup EXIT

mkdir -p "$MOUNT_DIR" "$(dirname "$EVIDENCE_PATH")"
hdiutil attach -readonly -nobrowse -mountpoint "$MOUNT_DIR" "$DMG_PATH" >/dev/null
MOUNTED=1

MOUNTED_APP="$MOUNT_DIR/VidChopper.app"
MOUNTED_CLI_DIR="$MOUNT_DIR/VidChopperCLI"
for required_path in \
    "$MOUNTED_APP/Contents/MacOS/VidChopper" \
    "$MOUNTED_CLI_DIR/VidChopperCLI" \
    "$MOUNTED_CLI_DIR/Install CLI.command" \
    "$MOUNT_DIR/Applications" \
    "$MOUNT_DIR/LICENSE" \
    "$MOUNT_DIR/README.txt" \
    "$MOUNT_DIR/THIRD_PARTY_NOTICES.txt" \
    "$MOUNT_DIR/TRUST_GUIDANCE.txt"; do
    if [[ ! -e "$required_path" && ! -L "$required_path" ]]; then
        echo "DMG inventory is missing: $required_path" >&2
        exit 1
    fi
done

codesign --verify --deep --strict "$MOUNTED_APP"
codesign --verify --strict "$MOUNTED_CLI_DIR/VidChopperCLI"

if [[ "$(lipo -archs "$MOUNTED_APP/Contents/MacOS/VidChopper")" != "arm64" ]]; then
    echo "Packaged GUI is not arm64-only." >&2
    exit 1
fi
if [[ "$(lipo -archs "$MOUNTED_CLI_DIR/VidChopperCLI")" != "arm64" ]]; then
    echo "Packaged CLI is not arm64-only." >&2
    exit 1
fi

audit_macho_dependencies() {
    local root="$1"
    local audit_path="$2"
    local install_id
    : > "$audit_path"
    while IFS= read -r -d '' file_path; do
        if file "$file_path" | rg -q 'Mach-O'; then
            install_id="$(otool -D "$file_path" 2>/dev/null | sed -n '2p')"
            printf '%s:\n' "$file_path" >> "$audit_path"
            while IFS= read -r dependency_line; do
                dependency_path="${dependency_line%% (*}"
                dependency_path="${dependency_path#${dependency_path%%[![:space:]]*}}"
                if [[ "$dependency_path" != "$install_id" ]]; then
                    printf '%s\n' "$dependency_line" >> "$audit_path"
                    if [[ "$dependency_path" == @rpath/* ]]; then
                        dependency_suffix="${dependency_path#@rpath/}"
                        if [[ -z "$(find "$root" -path "*/$dependency_suffix" -print -quit)" ]]; then
                            echo "Unresolved bundled dependency '$dependency_path' from $file_path" >&2
                            exit 1
                        fi
                    elif [[ "$dependency_path" == /* \
                        && "$dependency_path" != /System/Library/* \
                        && "$dependency_path" != /usr/lib/* ]]; then
                        echo "Unexpected absolute dependency '$dependency_path' from $file_path" >&2
                        exit 1
                    fi
                fi
            done < <(otool -L "$file_path" | tail -n +2)
        fi
    done < <(find "$root" -type f -print0)
    if rg -n '/Users/|/home/|/opt/homebrew|/usr/local/(Cellar|opt)/|/build/' "$audit_path"; then
        echo "Packaged Mach-O dependency audit found a developer-local path." >&2
        exit 1
    fi
}

audit_macho_dependencies "$MOUNTED_APP" "$WORK_DIR/app-dependencies.txt"
audit_macho_dependencies "$MOUNTED_CLI_DIR" "$WORK_DIR/dmg-cli-dependencies.txt"

COPIED_APP="$WORK_DIR/relocated/VidChopper.app"
mkdir -p "$(dirname "$COPIED_APP")"
ditto "$MOUNTED_APP" "$COPIED_APP"
hdiutil detach "$MOUNT_DIR" -quiet
MOUNTED=0
codesign --verify --deep --strict "$COPIED_APP"

EXTRACT_DIR="$WORK_DIR/extracted-cli"
mkdir -p "$EXTRACT_DIR"
tar -xzf "$CLI_ARCHIVE_PATH" -C "$EXTRACT_DIR"
CLI="$EXTRACT_DIR/VidChopperCLI/VidChopperCLI"
if [[ ! -x "$CLI" ]]; then
    echo "Extracted CLI was not found at $CLI" >&2
    exit 1
fi
codesign --verify --strict "$CLI"
audit_macho_dependencies "$EXTRACT_DIR/VidChopperCLI" "$WORK_DIR/archive-cli-dependencies.txt"

CLI_VERSION_OUTPUT="$($CLI --version)"
if [[ "$CLI_VERSION_OUTPUT" != "VidChopperCLI $VERSION" ]]; then
    echo "Packaged CLI version mismatch: $CLI_VERSION_OUTPUT" >&2
    exit 1
fi
PLIST_VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "$COPIED_APP/Contents/Info.plist")"
if [[ "$PLIST_VERSION" != "$VERSION" ]]; then
    echo "Packaged app version mismatch: $PLIST_VERSION" >&2
    exit 1
fi

MEDIA_DIR="$WORK_DIR/media"
TEST_HOME="$WORK_DIR/clean-home"
mkdir -p "$MEDIA_DIR" "$TEST_HOME"
SOURCE_VIDEO="$MEDIA_DIR/candidate-smoke.mp4"
CHAPTER_FILE="$MEDIA_DIR/candidate-smoke.chapters.json"
ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i testsrc=size=320x180:rate=24 -t 3 -pix_fmt yuv420p \
    "$SOURCE_VIDEO"
cat > "$CHAPTER_FILE" <<'JSON'
{
  "version": 1,
  "output": {
    "folder": "%source%_chapters",
    "namingPattern": "%index% - %name%"
  },
  "chapters": [
    {
      "name": "Candidate Smoke",
      "start": "00:00:00.000",
      "end": "00:00:02.000"
    }
  ]
}
JSON

READY_FILE="$WORK_DIR/gui-ready.txt"
GUI_LOG="$WORK_DIR/gui.log"
HOME="$TEST_HOME" "$COPIED_APP/Contents/MacOS/VidChopper" \
    --demo-scene=workspace \
    --demo-source="$SOURCE_VIDEO" \
    --window-size=800x600 \
    --demo-ready-file="$READY_FILE" \
    > "$GUI_LOG" 2>&1 &
GUI_PID=$!
for _ in $(seq 1 150); do
    if [[ -f "$READY_FILE" ]]; then
        break
    fi
    if ! kill -0 "$GUI_PID" 2>/dev/null; then
        echo "Relocated GUI exited before writing its ready marker." >&2
        tail -n 100 "$GUI_LOG" >&2
        exit 1
    fi
    sleep 0.2
done
if [[ ! -f "$READY_FILE" || "$(tr -d '\r\n' < "$READY_FILE")" != "ready" ]]; then
    echo "Relocated GUI did not write the expected ready marker." >&2
    tail -n 100 "$GUI_LOG" >&2
    exit 1
fi
kill "$GUI_PID" 2>/dev/null || true
wait "$GUI_PID" 2>/dev/null || true
GUI_PID=""

DRY_RUN_LOG="$WORK_DIR/cli-dry-run.log"
HOME="$TEST_HOME" "$CLI" "$SOURCE_VIDEO" "$CHAPTER_FILE" --dry-run 2>&1 | tee "$DRY_RUN_LOG"
if ! rg -q 'Planned chapters: 1' "$DRY_RUN_LOG"; then
    echo "Packaged CLI dry-run did not plan exactly one chapter." >&2
    exit 1
fi
OUTPUT_DIR="$MEDIA_DIR/candidate-smoke_chapters"
NATIVE_CLI_SETTINGS="$TEST_HOME/Library/Application Support/VidChopper/VidChopperCLI.ini"
if [[ -e "$OUTPUT_DIR" || -e "$NATIVE_CLI_SETTINGS" || -e "$EXTRACT_DIR/VidChopperCLI/VidChopperCLI.ini" ]]; then
    echo "Packaged CLI dry-run created settings or output artifacts." >&2
    exit 1
fi

EXPORT_LOG="$WORK_DIR/cli-export.log"
HOME="$TEST_HOME" "$CLI" "$SOURCE_VIDEO" "$CHAPTER_FILE" --preset ultrafast --crf 40 2>&1 | tee "$EXPORT_LOG"
if ! rg -q 'Summary: exported=1, failed=0, skipped=0' "$EXPORT_LOG"; then
    echo "Packaged CLI export summary did not report complete success." >&2
    exit 1
fi

MANIFEST_PATH="$OUTPUT_DIR/vidchopper-manifest.json"
jq -e '
    .jobStatus == "success" and
    (.segments | length == 1) and
    all(.segments[]; .processState == "success" and .durationVerified == true)
' "$MANIFEST_PATH" >/dev/null
OUTPUT_PATH="$(jq -r '.segments[0].outputPath' "$MANIFEST_PATH")"
if [[ ! -f "$OUTPUT_PATH" ]]; then
    echo "Packaged CLI output is missing: $OUTPUT_PATH" >&2
    exit 1
fi
ACTUAL_DURATION="$(ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 "$OUTPUT_PATH")"
awk -v actual="$ACTUAL_DURATION" 'BEGIN { difference = actual - 2.0; if (difference < 0) difference = -difference; exit difference > 1.0 }'

VIDEOTOOLBOX_STATUS="unavailable-physical-required"
if ffmpeg -hide_banner -loglevel error -y \
    -f lavfi -i color=c=black:s=64x64:r=1 -frames:v 1 \
    -c:v hevc_videotoolbox "$WORK_DIR/videotoolbox-smoke.mp4" \
    > "$WORK_DIR/videotoolbox.log" 2>&1; then
    VIDEOTOOLBOX_STATUS="passed"
fi

if [[ "$(shasum -a 256 "$DMG_PATH" | awk '{ print $1 }')" != "$ORIGINAL_DMG_HASH" || \
      "$(shasum -a 256 "$CLI_ARCHIVE_PATH" | awk '{ print $1 }')" != "$ORIGINAL_CLI_HASH" ]]; then
    echo "Candidate bytes changed during verification." >&2
    exit 1
fi

FFMPEG_PATH="$(command -v ffmpeg)"
FFPROBE_PATH="$(command -v ffprobe)"
FFMPEG_VERSION="$(ffmpeg -version | sed -n '1p')"
FFPROBE_VERSION="$(ffprobe -version | sed -n '1p')"
OS_VERSION="$(sw_vers -productVersion)"
OS_BUILD="$(sw_vers -buildVersion)"
MACHINE_MODEL="$(sysctl -n hw.model)"
CPU_MODEL="$(sysctl -n machdep.cpu.brand_string)"
DMG_SIZE="$(stat -f '%z' "$DMG_PATH")"
CLI_SIZE="$(stat -f '%z' "$CLI_ARCHIVE_PATH")"
SOURCE_COMMIT="${GITHUB_SHA:-$(git -C "$(dirname "${BASH_SOURCE[0]}")/.." rev-parse HEAD 2>/dev/null || printf unknown)}"
WORKFLOW_RUN_ID="${GITHUB_RUN_ID:-local}"
WORKFLOW_RUN_ATTEMPT="${GITHUB_RUN_ATTEMPT:-local}"
TESTER="${GITHUB_ACTOR:-${USER:-unknown}}"
QT_VERSION="${QT_VERSION:-unknown}"
TIMESTAMP="$(date -u '+%Y-%m-%dT%H:%M:%SZ')"

jq -n \
    --arg version "$VERSION" \
    --arg sourceCommit "$SOURCE_COMMIT" \
    --arg workflowRunId "$WORKFLOW_RUN_ID" \
    --arg workflowRunAttempt "$WORKFLOW_RUN_ATTEMPT" \
    --arg osVersion "$OS_VERSION" \
    --arg osBuild "$OS_BUILD" \
    --arg architecture "arm64" \
    --arg machineModel "$MACHINE_MODEL" \
    --arg cpuModel "$CPU_MODEL" \
    --arg qtVersion "$QT_VERSION" \
    --arg ffmpegPath "$FFMPEG_PATH" \
    --arg ffmpegVersion "$FFMPEG_VERSION" \
    --arg ffprobePath "$FFPROBE_PATH" \
    --arg ffprobeVersion "$FFPROBE_VERSION" \
    --arg tester "$TESTER" \
    --arg timestamp "$TIMESTAMP" \
    --arg dmgName "$(basename "$DMG_PATH")" \
    --arg dmgSha256 "$ORIGINAL_DMG_HASH" \
    --argjson dmgSize "$DMG_SIZE" \
    --arg cliName "$(basename "$CLI_ARCHIVE_PATH")" \
    --arg cliSha256 "$ORIGINAL_CLI_HASH" \
    --argjson cliSize "$CLI_SIZE" \
    --arg videoToolbox "$VIDEOTOOLBOX_STATUS" \
    '{
      schemaVersion: 1,
      productVersion: $version,
      sourceCommit: $sourceCommit,
      workflow: { runId: $workflowRunId, attempt: $workflowRunAttempt },
      host: {
        os: "macOS",
        version: $osVersion,
        build: $osBuild,
        architecture: $architecture,
        machineModel: $machineModel,
        cpuModel: $cpuModel
      },
      tools: {
        qt: $qtVersion,
        ffmpeg: { path: $ffmpegPath, version: $ffmpegVersion },
        ffprobe: { path: $ffprobePath, version: $ffprobeVersion }
      },
      artifacts: [
        { name: $dmgName, size: $dmgSize, sha256: $dmgSha256 },
        { name: $cliName, size: $cliSize, sha256: $cliSha256 }
      ],
      tests: [
        { name: "checksum-pairs", outcome: "passed" },
        { name: "dmg-inventory", outcome: "passed" },
        { name: "ad-hoc-signatures", outcome: "passed" },
        { name: "dependency-audit", outcome: "passed" },
        { name: "relocated-app-ready-marker", outcome: "passed" },
        { name: "cli-version", outcome: "passed" },
        { name: "cli-dry-run", outcome: "passed" },
        { name: "cli-x264-export", outcome: "passed" },
        { name: "manifest-and-duration", outcome: "passed" },
        { name: "videotoolbox-runner-capability", outcome: $videoToolbox }
      ],
      physicalAcceptanceRequired: true,
      tester: $tester,
      timestamp: $timestamp,
      outcome: "passed"
    }' > "$EVIDENCE_PATH"

echo "macOS candidate verification passed."
echo "Evidence: $EVIDENCE_PATH"
