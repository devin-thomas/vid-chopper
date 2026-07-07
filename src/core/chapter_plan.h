#pragma once

#include "core/models.h"

#include <string>
#include <vector>

namespace vidchopper {

struct ValidationIssue {
    u16 chapter_index {0};
    std::string message;

    [[nodiscard]] auto operator==(const ValidationIssue&) const -> bool = default;
};

struct ValidationResult {
    std::vector<ValidationIssue> issues;

    [[nodiscard]] auto ok() const noexcept -> bool;
    [[nodiscard]] auto operator==(const ValidationResult&) const -> bool = default;
};

[[nodiscard]] auto build_default_chapters(u64 duration_ms, u8 requested_count) -> std::vector<ChapterSegment>;
[[nodiscard]] auto validate_chapters(
    const std::vector<ChapterSegment>& chapters, u64 duration_ms, const ExportSettings& settings) -> ValidationResult;
[[nodiscard]] auto default_output_directory(const Path& source_path, const ExportSettings& settings) -> Path;

} // namespace vidchopper
