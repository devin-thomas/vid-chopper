#include "cli/export_runner.hpp"

#include <filesystem>
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
            ProcessResult process = executor_(make_process_request(segment.command, options));
            const bool succeeded = process.ok();
            if (!succeeded) {
                record_failure(result, process.state);
            }

            job_result.segments.push_back(RenderedSegment {
                .source_path = job.metadata.source_path,
                .chapter_index = segment.chapter_index,
                .chapter_name = segment.chapter.name,
                .output_path = segment.output_path,
                .process = std::move(process),
            });

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
