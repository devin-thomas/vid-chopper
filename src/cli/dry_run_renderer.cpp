#include "cli/dry_run_renderer.hpp"

#include "cli/command_display.hpp"
#include "core/path_utils.hpp"

#include <filesystem>
#include <format>
#include <ostream>
#include <optional>
#include <system_error>

namespace vidchopper {

namespace {

[[nodiscard]] auto overwrite_name(const OverwriteMode mode) -> std::string_view {
    switch (mode) {
    case OverwriteMode::Ask:
        return "ask";
    case OverwriteMode::Overwrite:
        return "overwrite";
    case OverwriteMode::Skip:
        return "skip";
    }
    return "unknown";
}

} // namespace

auto DryRunRenderResult::ok() const noexcept -> bool {
    return success && errors.empty();
}

auto render_dry_run(const std::vector<ResolvedExportJob>& jobs,
    std::ostream& output,
    std::ostream& error_output,
    const std::optional<Path>& aggregate_json_path,
    const std::optional<Path>& aggregate_csv_path) -> DryRunRenderResult {
    auto result = DryRunRenderResult {.success = true};
    auto chapter_count = size_t {0};
    output << "Dry run: enabled. No settings, output, manifest, or media artifacts will be created.\n";
    output << "Probing: completed as required to validate the plan.\n";
    output << "Jobs: " << jobs.size() << "\n";
    if (aggregate_json_path.has_value()) {
        output << "  Aggregate manifest JSON: " << path_to_utf8(*aggregate_json_path) << "\n";
    }
    if (aggregate_csv_path.has_value()) {
        output << "  Aggregate manifest CSV: " << path_to_utf8(*aggregate_csv_path) << "\n";
    }

    for (size_t job_index = 0; job_index < jobs.size(); ++job_index) {
        const ResolvedExportJob& job = jobs[job_index];
        output << std::format("Job {}/{}\n", job_index + 1, jobs.size());
        output << "  Source: " << path_to_utf8(job.metadata.source_path) << "\n";
        if (job.uses_embedded_chapters) {
            output << "  ChapterSource: embedded chapters\n";
        } else if (job.chapter_source_path.has_value()) {
            output << "  ChapterSource: " << path_to_utf8(*job.chapter_source_path) << "\n";
        } else {
            output << "  ChapterSource: unspecified\n";
        }
        output << "  Output directory: " << path_to_utf8(job.output_directory) << "\n";
        output << "  Effective settings: CRF=" << static_cast<int>(job.settings.x264_crf)
               << ", CQ=" << static_cast<int>(job.settings.nvenc_cq) << ", preset=x264:" << job.settings.x264_preset
               << ", threads=" << static_cast<int>(job.settings.ffmpeg_threads)
               << ", overwrite=" << overwrite_name(job.settings.overwrite_mode)
               << ", stop-on-first-error=" << (job.settings.stop_on_first_error ? "yes" : "no") << "\n";
        if (job.settings.write_json_manifest) {
            output << "  Manifest JSON: " << path_to_utf8(job.output_directory / "vidchopper-manifest.json") << "\n";
        }
        if (job.settings.write_csv_manifest) {
            output << "  Manifest CSV: " << path_to_utf8(job.output_directory / "vidchopper-manifest.csv") << "\n";
        }

        for (size_t segment_index = 0; segment_index < job.segments.size(); ++segment_index) {
            const PlannedExportSegment& segment = job.segments[segment_index];
            ++chapter_count;
            auto path_error = std::error_code {};
            const bool exists = std::filesystem::exists(segment.output_path, path_error);
            if (path_error) {
                const std::string message = "Could not inspect planned output '" + path_to_utf8(segment.output_path)
                    + "': " + path_error.message();
                result.errors.push_back(message);
                error_output << message << "\n";
                continue;
            }
            output << std::format(
                "  Chapter {}/{}: {}\n", segment_index + 1, job.segments.size(), segment.chapter.name);
            output << "    Segment: " << path_to_utf8(segment.output_path) << "\n";
            output << "    Existing output: " << (exists ? "yes" : "no") << "\n";
            output << "    Command: " << display_command(segment.command) << "\n";
        }
    }

    output << "Planned chapters: " << chapter_count << "\n";
    if (!result.errors.empty()) {
        result.success = false;
    }
    return result;
}

} // namespace vidchopper
