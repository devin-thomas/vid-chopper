#include "cli/export_runner.hpp"

#include <filesystem>
#include <format>
#include <system_error>
#include <utility>

namespace vidchopper {

namespace {

auto record_failure(ExportRunResult& result, const ProcessExitState state) -> void {
    if (state == ProcessExitState::FailedStart) {
        result.exit_code = ExportExitCode::ToolingError;
    } else if (result.exit_code == ExportExitCode::Success) {
        result.exit_code = ExportExitCode::ExportFailure;
    }
}

[[nodiscard]] auto make_process_request(
    const std::vector<std::string>& command, const ExportRunOptions& options) -> ProcessRequest {
    return ProcessRequest {
        .executable = command.front(),
        .arguments = {command.begin() + 1, command.end()},
        .timeout = options.process_timeout,
        .stdout_limit_bytes = options.stdout_limit_bytes,
        .stderr_limit_bytes = options.stderr_limit_bytes,
    };
}

[[nodiscard]] auto bounded_detail(const ProcessResult& process, const size_t limit) -> std::string {
    std::string detail = process.error_message.empty() ? process.standard_error : process.error_message;
    if (detail.size() > limit) {
        detail.resize(limit);
        detail += "... [truncated]";
    }
    return detail;
}

auto add_failure_context(ProcessResult& process,
    const ResolvedExportJob& job,
    const PlannedExportSegment& segment,
    const size_t detail_limit) -> void {
    const std::string detail = bounded_detail(process, detail_limit);
    process.error_message = std::format("ffmpeg executable '{}' failed for source '{}', chapter {}, output '{}' ({}, "
                                        "exit code {})",
        segment.command.front(),
        job.metadata.source_path.string(),
        segment.chapter_index + 1,
        segment.output_path.string(),
        process_exit_state_name(process.state),
        process.exit_code);
    if (!detail.empty()) {
        process.error_message += ": " + detail;
    }
}

} // namespace

auto RenderedSegment::ok() const noexcept -> bool {
    return process.ok();
}

auto ExportJobResult::ok() const noexcept -> bool {
    if (!error_message.empty() || stopped_early) {
        return false;
    }
    for (const RenderedSegment& segment : segments) {
        if (!segment.ok()) {
            return false;
        }
    }
    return true;
}

auto ExportRunResult::ok() const noexcept -> bool {
    return exit_code == ExportExitCode::Success;
}

ExportRunner::ExportRunner(ProcessExecutor executor)
    : executor_ {std::move(executor)} {
}

auto ExportRunner::run(
    const std::vector<ResolvedExportJob>& jobs, const ExportRunOptions& options) const -> ExportRunResult {
    auto result = ExportRunResult {};
    result.jobs.reserve(jobs.size());

    for (const ResolvedExportJob& job : jobs) {
        const bool stop_on_first_error = job.settings.stop_on_first_error;
        auto job_result = ExportJobResult {.source_path = job.metadata.source_path};
        job_result.segments.reserve(job.segments.size());

        if (job.segments.empty()) {
            job_result.error_message = "Export job has no planned segments.";
            result.exit_code = ExportExitCode::ExportFailure;
            result.jobs.push_back(std::move(job_result));
            if (stop_on_first_error) {
                result.stopped_early = result.jobs.size() < jobs.size();
                break;
            }
            continue;
        }

        for (const PlannedExportSegment& segment : job.segments) {
            if (segment.command.empty()) {
                job_result.error_message = "Export job contains an empty planned command.";
                result.exit_code = ExportExitCode::ExportFailure;
                break;
            }
        }
        if (!job_result.error_message.empty()) {
            result.jobs.push_back(std::move(job_result));
            if (stop_on_first_error) {
                result.stopped_early = result.jobs.size() < jobs.size();
                break;
            }
            continue;
        }

        auto directory_error = std::error_code {};
        std::filesystem::create_directories(job.output_directory, directory_error);
        if (directory_error) {
            job_result.error_message = "Failed to create output directory: " + directory_error.message();
            if (result.exit_code == ExportExitCode::Success) {
                result.exit_code = ExportExitCode::ExportFailure;
            }
            result.jobs.push_back(std::move(job_result));
            if (stop_on_first_error) {
                result.stopped_early = true;
                break;
            }
            continue;
        }

        for (auto index = size_t {0}; index < job.segments.size(); ++index) {
            const PlannedExportSegment& segment = job.segments[index];
            if (options.segment_started) {
                options.segment_started(
                    result.jobs.size() + 1, jobs.size(), index + 1, job.segments.size(), job, segment);
            }
            auto path_error = std::error_code {};
            const bool output_exists = std::filesystem::exists(segment.output_path, path_error);
            if (path_error) {
                job_result.error_message =
                    "Could not inspect planned output '" + segment.output_path.string() + "': " + path_error.message();
                result.exit_code = ExportExitCode::ExportFailure;
                break;
            }
            if (output_exists && job.settings.overwrite_mode == OverwriteMode::Skip) {
                const auto skipped = RenderedSegment {
                    .source_path = job.metadata.source_path,
                    .chapter_index = segment.chapter_index,
                    .chapter_name = segment.chapter.name,
                    .output_path = segment.output_path,
                    .process = ProcessResult {.state = ProcessExitState::Success},
                    .skipped = true,
                };
                if (options.message) {
                    options.message("Skipping existing output: " + segment.output_path.string());
                }
                job_result.segments.push_back(skipped);
                if (options.segment_finished) {
                    options.segment_finished(job_result.segments.back());
                }
                continue;
            }
            if (output_exists && options.message) {
                options.message("Overwriting existing output: " + segment.output_path.string());
            }
            ProcessResult process = executor_(make_process_request(segment.command, options));
            const bool succeeded = process.ok();
            if (!succeeded) {
                record_failure(result, process.state);
                add_failure_context(process, job, segment, options.stderr_limit_bytes);
            }

            job_result.segments.push_back(RenderedSegment {
                .source_path = job.metadata.source_path,
                .chapter_index = segment.chapter_index,
                .chapter_name = segment.chapter.name,
                .output_path = segment.output_path,
                .process = std::move(process),
                .overwrote_existing = output_exists,
            });
            if (options.segment_finished) {
                options.segment_finished(job_result.segments.back());
            }

            if (!succeeded && stop_on_first_error) {
                job_result.stopped_early = index + 1 < job.segments.size();
                result.stopped_early = job_result.stopped_early || result.jobs.size() + 1 < jobs.size();
                break;
            }
        }

        const bool stopped = stop_on_first_error && !job_result.ok();
        result.jobs.push_back(std::move(job_result));
        if (stopped) {
            break;
        }
    }

    return result;
}

} // namespace vidchopper
