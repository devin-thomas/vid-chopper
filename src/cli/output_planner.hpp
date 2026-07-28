#pragma once

#include "cli/export_runner.hpp"

#include <string>
#include <vector>

namespace vidchopper {

struct OutputPlanInput {
    VideoMetadata metadata;
    std::vector<ChapterSegment> chapters;
    ExportSettings settings;
    EncoderEnvironment environment;
};

struct OutputPlanResult {
    std::vector<ResolvedExportJob> jobs;
    std::vector<std::string> errors;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

[[nodiscard]] auto plan_outputs(const std::vector<OutputPlanInput>& inputs) -> OutputPlanResult;

} // namespace vidchopper
