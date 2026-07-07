#pragma once

#include "core/models.h"
#include "core/types.h"

#include <string>
#include <vector>

namespace vidchopper {

struct ChapterConfig {
    std::vector<ChapterSegment> chapters;
    ExportSettings settings;

    [[nodiscard]] auto operator==(const ChapterConfig&) const -> bool = default;
};

struct ChapterConfigLoadResult {
    bool success {false};
    ChapterConfig config;
    std::string error_message;

    [[nodiscard]] auto ok() const noexcept -> bool;
    [[nodiscard]] auto operator==(const ChapterConfigLoadResult&) const -> bool = default;
};

[[nodiscard]] auto load_chapter_config(
    const Path& config_path, u64 source_duration_ms, const ExportSettings& base_settings) -> ChapterConfigLoadResult;

} // namespace vidchopper
