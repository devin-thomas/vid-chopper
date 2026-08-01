#include "cli/output_planner.hpp"

#include "core/chapter_plan.hpp"
#include "core/command_builder.hpp"
#include "core/string_utils.hpp"

#include <format>
#include <unordered_map>
#include <utility>

namespace vidchopper {

namespace {

struct PlannedOwner {
    Path source_path;
    u16 chapter_index {0};
};

[[nodiscard]] auto collision_key(const Path& path) -> std::string {
    return to_lower_copy(path.lexically_normal().generic_string());
}

[[nodiscard]] auto describe_owner(const PlannedOwner& owner) -> std::string {
    return std::format("source '{}' chapter {}", owner.source_path.string(), owner.chapter_index + 1);
}

} // namespace

auto OutputPlanResult::ok() const noexcept -> bool {
    return errors.empty();
}

auto plan_outputs(const std::vector<OutputPlanInput>& inputs) -> OutputPlanResult {
    auto result = OutputPlanResult {};
    if (inputs.empty()) {
        result.errors.push_back("At least one output plan input is required.");
        return result;
    }
    result.jobs.reserve(inputs.size());
    auto owners = std::unordered_map<std::string, PlannedOwner> {};

    for (const OutputPlanInput& input : inputs) {
        const ValidationResult validation =
            validate_chapters(input.chapters, input.metadata.duration_ms, input.settings);
        if (!validation.ok()) {
            for (const ValidationIssue& issue : validation.issues) {
                result.errors.push_back(std::format("Invalid output plan for source '{}', chapter {}: {}",
                    input.metadata.source_path.string(),
                    issue.chapter_index + 1,
                    issue.message));
            }
            continue;
        }

        const Path output_directory = default_output_directory(input.metadata.source_path, input.settings);
        auto job = ResolvedExportJob {
            .metadata = input.metadata,
            .chapter_source_path = input.chapter_source_path,
            .uses_embedded_chapters = input.uses_embedded_chapters,
            .output_directory = output_directory,
            .settings = input.settings,
            .environment = input.environment,
        };
        job.segments.reserve(input.chapters.size());

        for (auto index = size_t {0}; index < input.chapters.size(); ++index) {
            const auto chapter_index = static_cast<u16>(index);
            const ChapterSegment& chapter = input.chapters[index];
            const Path output_path =
                output_path_for(input.metadata, chapter, chapter_index, output_directory, input.settings);
            const std::string key = collision_key(output_path);
            const auto [owner, inserted] = owners.emplace(
                key, PlannedOwner {.source_path = input.metadata.source_path, .chapter_index = chapter_index});
            if (!inserted) {
                const PlannedOwner current_owner {
                    .source_path = input.metadata.source_path,
                    .chapter_index = chapter_index,
                };
                result.errors.push_back(std::format("Output collision at '{}': {} conflicts with {}.",
                    output_path.string(),
                    describe_owner(current_owner),
                    describe_owner(owner->second)));
            }

            job.segments.push_back(PlannedExportSegment {
                .chapter = chapter,
                .chapter_index = chapter_index,
                .output_path = output_path,
                .command =
                    build_ffmpeg_command(input.metadata, chapter, output_path, input.settings, input.environment),
            });
        }

        result.jobs.push_back(std::move(job));
    }

    if (!result.errors.empty()) {
        result.jobs.clear();
    }
    return result;
}

} // namespace vidchopper
