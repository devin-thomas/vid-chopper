#pragma once

#include "core/models.hpp"
#include "core/types.hpp"

#include <string>
#include <vector>

namespace vidchopper {

struct ChapterConfig {
    u32 schema_version {1};
    std::vector<ChapterSegment> chapters {};
    ExportSettings settings {};

    [[nodiscard]] auto operator==(const ChapterConfig&) const -> bool = default;
};

struct ChapterConfigLoadResult {
    bool success {false};
    ChapterConfig config {};
    std::string error_message {};

    [[nodiscard]] auto ok() const noexcept -> bool;
    [[nodiscard]] auto operator==(const ChapterConfigLoadResult&) const -> bool = default;
};

[[nodiscard]] auto load_chapter_config(
    const Path& config_path, u64 source_duration_ms, const ExportSettings& base_settings) -> ChapterConfigLoadResult;

} // namespace vidchopper
