#include "services/export_engine.hpp"

#include "core/path_utils.hpp"
#include "services/probe_service.hpp"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <optional>
#include <string_view>
#include <system_error>
#include <utility>

namespace vidchopper {

namespace {

inline constexpr auto duration_tolerance_ms = u64 {1000};

auto record_failure(ExportRunResult& result, const ProcessExitState state) -> void {
    if (state == ProcessExitState::FailedStart) {
        result.exit_code = ExportExitCode::ToolingError;
    } else if (result.exit_code == ExportExitCode::Success) {
        result.exit_code = ExportExitCode::ExportFailure;
    }
    if (state == ProcessExitState::Cancelled) {
        result.cancelled = true;
    }
}

[[nodiscard]] auto make_process_request(const std::vector<std::string>& command,
    const ExportRunOptions& options,
    std::function<void(std::string_view)> progress_output,
    const Path& executable) -> ProcessRequest {
    return ProcessRequest {
        .executable = executable.empty() ? path_from_utf8(command.front()) : executable,
        .arguments = {command.begin() + 1, command.end()},
        .timeout = options.process_timeout,
        .stdout_limit_bytes = options.stdout_limit_bytes,
        .stderr_limit_bytes = options.stderr_limit_bytes,
        .stop_token = options.stop_token,
        .standard_output_chunk = std::move(progress_output),
        .standard_error_chunk = options.process_output,
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
        path_to_utf8(path_from_utf8(segment.command.front())),
        path_to_utf8(job.metadata.source_path),
        segment.chapter_index + 1,
        path_to_utf8(segment.output_path),
        process_exit_state_name(process.state),
        process.exit_code);
    if (!detail.empty()) {
        process.error_message += ": " + detail;
    }
}

[[nodiscard]] auto parse_progress_microseconds(const std::string_view line) -> std::optional<u64> {
    constexpr auto out_time_us = std::string_view {"out_time_us="};
    constexpr auto out_time_ms = std::string_view {"out_time_ms="};
    auto value = std::string_view {};
    if (line.starts_with(out_time_us)) {
        value = line.substr(out_time_us.size());
    } else if (line.starts_with(out_time_ms)) {
        // FFmpeg's historical out_time_ms field is also expressed in microseconds.
        value = line.substr(out_time_ms.size());
    } else {
        return std::nullopt;
    }

    auto parsed = u64 {0};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (error != std::errc {} || end != value.data() + value.size()) {
        return std::nullopt;
    }
    return parsed;
}

class ProgressParser final {
public:
    explicit ProgressParser(std::function<void(u64)> callback)
        : callback_ {std::move(callback)} {
    }

    auto consume(const std::string_view chunk) -> void {
        buffer_.append(chunk);
        while (true) {
            const size_t newline = buffer_.find('\n');
            if (newline == std::string::npos) {
                return;
            }

            auto line = std::string_view {buffer_.data(), newline};
            if (!line.empty() && line.back() == '\r') {
                line.remove_suffix(1);
            }
            if (const std::optional<u64> microseconds = parse_progress_microseconds(line); microseconds.has_value()) {
                callback_(*microseconds / 1000);
            }
            buffer_.erase(0, newline + 1);
        }
    }

private:
    std::string buffer_;
    std::function<void(u64)> callback_;
};

[[nodiscard]] auto total_duration(const std::vector<ResolvedExportJob>& jobs) -> u64 {
    auto total = u64 {0};
    for (const ResolvedExportJob& job : jobs) {
        for (const PlannedExportSegment& segment : job.segments) {
            total += segment.chapter.end_ms - segment.chapter.start_ms;
        }
    }
    return total;
}

auto report_progress(const ExportRunOptions& options, const u64 completed_ms, const u64 total_ms) -> void {
    if (!options.progress_changed) {
        return;
    }
    const auto bounded = (std::min)(completed_ms, total_ms);
    const int percent = total_ms == 0 ? 100 : static_cast<int>((bounded * 100) / total_ms);
    options.progress_changed(percent);
}

struct DurationVerification {
    bool success {true};
    bool verified {false};
    u64 actual_duration_ms {0};
    ProcessResult process;
    std::string error_message;
};

[[nodiscard]] auto verify_duration(const ResolvedExportJob& job,
    const PlannedExportSegment& segment,
    const ProcessExecutor& executor,
    const std::stop_token stop_token,
    const Path& ffprobe_executable) -> DurationVerification {
    if (!job.settings.verify_output_durations) {
        return {};
    }

    ProbeResult probe = ProbeService {executor}.probe(ffprobe_executable, segment.output_path, stop_token);
    if (!probe.ok()) {
        return DurationVerification {
            .success = false,
            .process = std::move(probe.process),
            .error_message = std::format(
                "Duration verification failed for '{}': {}", path_to_utf8(segment.output_path), probe.error_message),
        };
    }

    const u64 expected = segment.chapter.end_ms - segment.chapter.start_ms;
    const u64 actual = probe.metadata.duration_ms;
    const u64 delta = expected > actual ? expected - actual : actual - expected;
    if (delta > duration_tolerance_ms) {
        return DurationVerification {
            .success = false,
            .verified = true,
            .actual_duration_ms = actual,
            .process = std::move(probe.process),
            .error_message = std::format("Duration verification failed for '{}': expected {} ms, observed {} ms.",
                path_to_utf8(segment.output_path),
                expected,
                actual),
        };
    }

    return DurationVerification {
        .verified = true,
        .actual_duration_ms = actual,
        .process = std::move(probe.process),
    };
}

} // namespace

auto RenderedSegment::ok() const noexcept -> bool {
    return process.ok() && verification_error.empty();
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
    return exit_code == ExportExitCode::Success && !cancelled;
}

ExportEngine::ExportEngine(ProcessExecutor executor)
    : executor_ {std::move(executor)}
    , tool_resolver_ {ToolDiscoveryOptions {.executor = executor_}}
    , validate_tools_ {is_default_process_executor(executor_)} {
}

auto ExportEngine::run(const std::vector<ResolvedExportJob>& jobs, const ExportRunOptions& options) const
    -> ExportRunResult {
    auto result = ExportRunResult {};
    result.jobs.reserve(jobs.size());
    const u64 batch_duration_ms = total_duration(jobs);
    auto completed_duration_ms = u64 {0};
    report_progress(options, 0, batch_duration_ms);

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

        Path ffmpeg_executable = path_from_utf8(job.settings.ffmpeg_path);
        Path ffprobe_executable = path_from_utf8(job.settings.ffprobe_path);
        if (validate_tools_) {
            const ToolDiscoveryResult tools = tool_resolver_.resolve_pair(ffmpeg_executable, ffprobe_executable);
            if (!tools.ok()) {
                job_result.error_message = "Media tool discovery failed: " + tools.failure_reason;
                result.exit_code = ExportExitCode::ToolingError;
                result.jobs.push_back(std::move(job_result));
                if (stop_on_first_error) {
                    result.stopped_early = result.jobs.size() < jobs.size();
                    break;
                }
                continue;
            }
            ffmpeg_executable = tools.ffmpeg.selected_path;
            ffprobe_executable = tools.ffprobe.selected_path;
            if (options.message) {
                for (const std::string& warning : tools.warnings) {
                    options.message(warning);
                }
            }
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
            const u64 segment_duration_ms = segment.chapter.end_ms - segment.chapter.start_ms;
            if (options.segment_started) {
                options.segment_started(
                    result.jobs.size() + 1, jobs.size(), index + 1, job.segments.size(), job, segment);
            }

            if (options.stop_token.stop_requested()) {
                auto cancelled = RenderedSegment {
                    .source_path = job.metadata.source_path,
                    .chapter_index = segment.chapter_index,
                    .chapter_name = segment.chapter.name,
                    .output_path = segment.output_path,
                    .process =
                        ProcessResult {
                            .state = ProcessExitState::Cancelled,
                            .error_message = "Export cancellation was requested.",
                        },
                };
                job_result.segments.push_back(std::move(cancelled));
                if (options.segment_finished) {
                    options.segment_finished(job_result.segments.back());
                }
                record_failure(result, ProcessExitState::Cancelled);
                job_result.stopped_early = index + 1 < job.segments.size();
                result.stopped_early = job_result.stopped_early || result.jobs.size() + 1 < jobs.size();
                break;
            }

            auto path_error = std::error_code {};
            const bool output_exists = std::filesystem::exists(segment.output_path, path_error);
            if (path_error) {
                job_result.error_message =
                    "Could not inspect planned output '" + path_to_utf8(segment.output_path) + "': " + path_error.message();
                result.exit_code = ExportExitCode::ExportFailure;
                break;
            }
            if (output_exists && job.settings.overwrite_mode == OverwriteMode::Skip) {
                auto skipped = RenderedSegment {
                    .source_path = job.metadata.source_path,
                    .chapter_index = segment.chapter_index,
                    .chapter_name = segment.chapter.name,
                    .output_path = segment.output_path,
                    .process = ProcessResult {.state = ProcessExitState::Success},
                    .skipped = true,
                };
                if (options.message) {
                    options.message("Skipping existing output: " + path_to_utf8(segment.output_path));
                }
                job_result.segments.push_back(std::move(skipped));
                if (options.segment_finished) {
                    options.segment_finished(job_result.segments.back());
                }
                completed_duration_ms += segment_duration_ms;
                report_progress(options, completed_duration_ms, batch_duration_ms);
                continue;
            }
            if (output_exists && options.message) {
                options.message(
                    "Existing output will use the configured overwrite policy: " + path_to_utf8(segment.output_path));
            }

            auto parser = ProgressParser {[&options, &completed_duration_ms, batch_duration_ms, segment_duration_ms](
                                              const u64 elapsed_ms) {
                report_progress(
                    options, completed_duration_ms + (std::min)(elapsed_ms, segment_duration_ms), batch_duration_ms);
            }};
            ProcessResult process = executor_(make_process_request(
                segment.command,
                options,
                [&parser](const std::string_view chunk) { parser.consume(chunk); },
                ffmpeg_executable));
            bool succeeded = process.ok();
            auto verification = DurationVerification {};
            if (succeeded) {
                verification = verify_duration(job, segment, executor_, options.stop_token, ffprobe_executable);
                succeeded = verification.success;
                if (!succeeded) {
                    record_failure(result, verification.process.state);
                }
            } else {
                record_failure(result, process.state);
                add_failure_context(process, job, segment, options.stderr_limit_bytes);
            }

            job_result.segments.push_back(RenderedSegment {
                .source_path = job.metadata.source_path,
                .chapter_index = segment.chapter_index,
                .chapter_name = segment.chapter.name,
                .output_path = segment.output_path,
                .process = std::move(process),
                .duration_verified = verification.verified,
                .actual_duration_ms = verification.actual_duration_ms,
                .verification_error = std::move(verification.error_message),
                .overwrote_existing = output_exists && job.settings.overwrite_mode == OverwriteMode::Overwrite,
            });
            if (options.segment_finished) {
                options.segment_finished(job_result.segments.back());
            }

            completed_duration_ms += segment_duration_ms;
            report_progress(options, completed_duration_ms, batch_duration_ms);

            const bool cancelled = job_result.segments.back().process.state == ProcessExitState::Cancelled
                || verification.process.state == ProcessExitState::Cancelled;
            if (cancelled) {
                result.cancelled = true;
                job_result.stopped_early = index + 1 < job.segments.size();
                result.stopped_early = job_result.stopped_early || result.jobs.size() + 1 < jobs.size();
                break;
            }
            if (!succeeded && stop_on_first_error) {
                job_result.stopped_early = index + 1 < job.segments.size();
                result.stopped_early = job_result.stopped_early || result.jobs.size() + 1 < jobs.size();
                break;
            }
        }

        const bool stopped = result.cancelled || (stop_on_first_error && !job_result.ok());
        result.jobs.push_back(std::move(job_result));
        if (stopped) {
            break;
        }
    }

    if (!result.cancelled) {
        report_progress(options, batch_duration_ms, batch_duration_ms);
    }
    return result;
}

} // namespace vidchopper
