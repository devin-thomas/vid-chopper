#include "cli/chapter_source_policy.hpp"

#include <string_view>

namespace vidchopper {

namespace {

[[nodiscard]] auto quote_cli_argument(const std::string_view argument) -> std::string {
    auto quoted = std::string {"\""};
    auto backslash_count = size_t {0};
    for (const char character : argument) {
        if (character == '\\') {
            ++backslash_count;
            continue;
        }

        if (character == '"') {
            quoted.append(backslash_count * 2 + 1, '\\');
            quoted.push_back(character);
        } else {
            quoted.append(backslash_count, '\\');
            quoted.push_back(character);
        }
        backslash_count = 0;
    }
    quoted.append(backslash_count * 2, '\\');
    quoted.push_back('"');
    return quoted;
}

} // namespace

auto embedded_rerun_command(const Path& source_path) -> std::string {
    return "VidChopperCLI.exe " + quote_cli_argument(source_path.string()) + " --embedded";
}

auto chapter_source_guidance(const VideoMetadata& metadata) -> std::string {
    if (metadata.embedded_chapters.empty()) {
        return "No embedded chapters were found. A JSON or YAML chapter config is required.";
    }

    return "Embedded chapters were found. Rerun exactly:\n" + embedded_rerun_command(metadata.source_path);
}

auto select_embedded_sources(const VideoMetadataList& metadata) -> EmbeddedChapterSelection {
    auto selection = EmbeddedChapterSelection {};
    selection.selected.reserve(metadata.size());
    selection.skipped.reserve(metadata.size());
    for (const VideoMetadata& video : metadata) {
        if (video.embedded_chapters.empty()) {
            selection.skipped.push_back(video.source_path);
        } else {
            selection.selected.push_back(video);
        }
    }
    return selection;
}

} // namespace vidchopper
