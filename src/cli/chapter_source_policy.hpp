#pragma once

#include "core/models.hpp"
#include "core/types.hpp"

#include <string>
#include <vector>

namespace vidchopper {

using VideoMetadataList = std::vector<VideoMetadata>;

struct EmbeddedChapterSelection {
    VideoMetadataList selected;
    std::vector<Path> skipped;

    [[nodiscard]] auto operator==(const EmbeddedChapterSelection&) const -> bool = default;
};

[[nodiscard]] auto embedded_rerun_command(const Path& source_path) -> std::string;
[[nodiscard]] auto chapter_source_guidance(const VideoMetadata& metadata) -> std::string;
[[nodiscard]] auto select_embedded_sources(const VideoMetadataList& metadata) -> EmbeddedChapterSelection;

} // namespace vidchopper
