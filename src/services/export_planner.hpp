#pragma once

#include "services/export_engine.hpp"

#include <optional>
#include <string>
#include <vector>

namespace vidchopper {

struct OutputPlanInput {
    VideoMetadata metadata;
    std::optional<Path> chapter_source_path;
    std::optional<Path> output_directory;
    bool uses_embedded_chapters {false};
    std::vector<ChapterSegment> chapters;
    ExportSettings settings;
    EncoderEnvironment environment;
    std::optional<EncoderSelection> encoder_selection;
};

struct OutputPlanResult {
    std::vector<ResolvedExportJob> jobs;
    std::vector<std::string> errors;

    [[nodiscard]] auto ok() const noexcept -> bool;
    [[nodiscard]] auto operator==(const OutputPlanResult&) const -> bool = default;
};

[[nodiscard]] auto plan_outputs(const std::vector<OutputPlanInput>& inputs) -> OutputPlanResult;

} // namespace vidchopper
