#include "cli/chapter_source_policy.hpp"

#include "cli/command_display.hpp"
#include "core/path_utils.hpp"

namespace vidchopper {

auto embedded_rerun_command(const Path& source_path) -> std::string {
    return "VidChopperCLI.exe " + quote_command_argument(path_to_utf8(source_path)) + " --embedded";
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
