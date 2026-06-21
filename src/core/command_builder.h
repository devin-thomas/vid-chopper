#pragma once

#include "core/models.h"

#include <filesystem>
#include <vector>

namespace vidchopper {

[[nodiscard]] auto resolve_encoder(const ExportSettings& settings, const EncoderEnvironment& environment) -> ResolvedEncoder;
[[nodiscard]] auto output_extension_for(const VideoMetadata& metadata, const ExportSettings& settings) -> std::string;
[[nodiscard]] auto output_path_for(
    const VideoMetadata& metadata,
    const ChapterSegment& chapter,
    u16 chapter_index,
    const std::filesystem::path& output_directory,
    const ExportSettings& settings
) -> std::filesystem::path;
[[nodiscard]] auto build_ffmpeg_command(
    const VideoMetadata& metadata,
    const ChapterSegment& chapter,
    const std::filesystem::path& output_path,
    const ExportSettings& settings,
    const EncoderEnvironment& environment
) -> std::vector<std::string>;

} // namespace vidchopper
