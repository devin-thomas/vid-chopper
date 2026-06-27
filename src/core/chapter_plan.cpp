#include "core/chapter_plan.h"

#include "core/string_utils.h"

#include <algorithm>

namespace vidchopper {

auto ValidationResult::ok() const noexcept -> bool {
    return issues.empty();
}

auto build_default_chapters(const u64 duration_ms, const u8 requested_count) -> std::vector<ChapterSegment> {
    if (duration_ms < 1000 || requested_count == 0) {
        return {};
    }

    const auto max_count = static_cast<u64>(requested_count);
    const auto safe_count = std::min<u64>(max_count, duration_ms / 1000);
    if (safe_count == 0) {
        return {};
    }

    auto chapters = std::vector<ChapterSegment> {};
    chapters.reserve(static_cast<usize>(safe_count));

    auto start_ms = u64 {0};
    for (auto index = u64 {0}; index < safe_count; ++index) {
        const auto is_last = index + 1 == safe_count;
        const auto end_ms = is_last ? duration_ms : ((duration_ms * (index + 1)) / safe_count);

        chapters.push_back(ChapterSegment {
            .name = "Chapter " + std::to_string(index + 1),
            .start_ms = start_ms,
            .end_ms = end_ms,
        });

        start_ms = end_ms;
    }

    return chapters;
}

auto validate_chapters(const std::vector<ChapterSegment>& chapters, const u64 duration_ms, const ExportSettings& settings) -> ValidationResult {
    auto result = ValidationResult {};
    const auto min_duration_ms = static_cast<u64>(settings.min_chapter_seconds) * 1000;

    if (chapters.empty()) {
        result.issues.push_back(ValidationIssue {.message = "At least one chapter is required."});
        return result;
    }

    if (chapters.size() > settings.max_chapters) {
        result.issues.push_back(ValidationIssue {.message = "Chapter count exceeds the 255 chapter limit."});
    }

    auto previous_end_ms = u64 {0};
    for (auto index = usize {0}; index < chapters.size(); ++index) {
        const auto& chapter = chapters[index];
        const auto trimmed_name = trim_copy(chapter.name);

        if (trimmed_name.empty()) {
            result.issues.push_back(ValidationIssue {
                .chapter_index = static_cast<u16>(index),
                .message = "Chapter names cannot be blank.",
            });
        }

        if (chapter.end_ms <= chapter.start_ms) {
            result.issues.push_back(ValidationIssue {
                .chapter_index = static_cast<u16>(index),
                .message = "Chapter end time must be after the start time.",
            });
            continue;
        }

        if ((chapter.end_ms - chapter.start_ms) < min_duration_ms) {
            result.issues.push_back(ValidationIssue {
                .chapter_index = static_cast<u16>(index),
                .message = "Each chapter must be at least one second long.",
            });
        }

        if (chapter.end_ms > duration_ms) {
            result.issues.push_back(ValidationIssue {
                .chapter_index = static_cast<u16>(index),
                .message = "Chapter end time exceeds the source duration.",
            });
        }

        if (index > 0 && chapter.start_ms < previous_end_ms) {
            result.issues.push_back(ValidationIssue {
                .chapter_index = static_cast<u16>(index),
                .message = "Chapters cannot overlap or move backward in time.",
            });
        }

        previous_end_ms = chapter.end_ms;
    }

    return result;
}

auto default_output_directory(const std::filesystem::path& source_path, const ExportSettings& settings) -> std::filesystem::path {
    auto folder_name = replace_all_copy(settings.output_folder_pattern, "%source%", source_path.stem().string());
    folder_name = sanitize_file_component(folder_name);
    return source_path.parent_path() / folder_name;
}

} // namespace vidchopper
