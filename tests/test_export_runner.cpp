#include "core/path_utils.hpp"
#include "services/export_engine.hpp"
#include "services/export_planner.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <stop_token>
#include <utility>
#include <vector>

using namespace vidchopper;

namespace {

[[nodiscard]] auto make_job(const Path& root, std::vector<ChapterSegment> chapters) -> ResolvedExportJob {
    auto settings = ExportSettings {};
    settings.ffmpeg_path = "/tmp/VidChopper/工具/ffmpeg";
    settings.overwrite_mode = OverwriteMode::Overwrite;
    settings.container_mode = ContainerMode::Mp4;
    settings.stop_on_first_error = false;
    settings.verify_output_durations = false;
    const auto input = OutputPlanInput {
        .metadata =
            VideoMetadata {
                .source_path = root / "source clips" / "match.mkv",
                .duration_ms = 6000,
                .frame_rate = FrameRate {.numerator = 60, .denominator = 1},
                .source_extension = ".mkv",
            },
        .chapters = std::move(chapters),
        .settings = settings,
        .environment = EncoderEnvironment {},
    };
    OutputPlanResult plan = plan_outputs({input});
    if (!plan.ok()) {
        test_support::fail("test export job should plan successfully");
    }
    return std::move(plan.jobs.front());
}

[[nodiscard]] auto chapters() -> std::vector<ChapterSegment> {
    return {
        {.name = "Intro", .start_ms = 0, .end_ms = 2000},
        {.name = "Match", .start_ms = 2000, .end_ms = 4000},
        {.name = "Outro", .start_ms = 4000, .end_ms = 6000},
    };
}

} // namespace

auto main() -> int {
    const Path root = std::filesystem::temp_directory_path() / "vidchopper-export-runner";
    std::filesystem::remove_all(root);

    auto observed_requests = std::vector<ProcessRequest> {};
    const auto successful_executor = [&observed_requests](const ProcessRequest& request) -> ProcessResult {
        observed_requests.push_back(request);
        return ProcessResult {.state = ProcessExitState::Success, .standard_output = "progress"};
    };
    const ResolvedExportJob successful_job = make_job(root, chapters());
    const auto capture_options = ExportRunOptions {
        .process_timeout = std::chrono::milliseconds {123},
        .stdout_limit_bytes = 17,
        .stderr_limit_bytes = 23,
    };
    const ExportRunResult successful = ExportEngine {successful_executor}.run({successful_job}, capture_options);
    test_support::expect_true(successful.ok(), "successful chapters should complete the run");
    test_support::expect_eq(successful.jobs.size(), size_t {1}, "one resolved job should produce one job result");
    test_support::expect_eq(
        successful.jobs.front().segments.size(), size_t {3}, "each chapter should produce a rendered segment");
    test_support::expect_eq(observed_requests.size(), size_t {3}, "chapters should run sequentially");
    test_support::expect_eq(
        observed_requests.front().timeout, capture_options.process_timeout, "runner should preserve process timeout");
    test_support::expect_eq(observed_requests.front().stdout_limit_bytes,
        capture_options.stdout_limit_bytes,
        "runner should preserve the stdout capture bound");
    test_support::expect_eq(observed_requests.front().stderr_limit_bytes,
        capture_options.stderr_limit_bytes,
        "runner should preserve the stderr capture bound");

    const RenderedSegment& first = successful.jobs.front().segments.front();
    test_support::expect_eq(first.source_path, successful_job.metadata.source_path, "result should retain source path");
    test_support::expect_eq(first.chapter_index, u16 {0}, "result should retain chapter index");
    test_support::expect_eq(first.chapter_name, std::string {"Intro"}, "result should retain chapter name");
    const std::vector<std::string>& expected_command = successful_job.segments.front().command;
    test_support::expect_eq(observed_requests.front().executable,
        path_from_utf8(expected_command.front()),
        "runner should preserve UTF-8 executable paths");
    test_support::expect_eq(observed_requests.front().arguments,
        std::vector<std::string> {expected_command.begin() + 1, expected_command.end()},
        "runner should preserve command-builder token order");

    ResolvedExportJob skip_job = make_job(root, chapters());
    std::filesystem::create_directories(skip_job.output_directory);
    std::ofstream {skip_job.segments.front().output_path} << "existing";
    skip_job.settings.overwrite_mode = OverwriteMode::Skip;
    auto skip_calls = size_t {0};
    const auto skip_executor = [&skip_calls](const ProcessRequest&) -> ProcessResult {
        ++skip_calls;
        return ProcessResult {.state = ProcessExitState::Success};
    };
    const ExportRunResult skipped = ExportEngine {skip_executor}.run({skip_job});
    test_support::expect_true(skipped.ok(), "skipped existing output should remain successful");
    test_support::expect_eq(skip_calls, size_t {2}, "skip mode should avoid the existing chapter process");
    test_support::expect_true(
        skipped.jobs.front().segments.front().skipped, "existing chapter should be marked skipped");

    ResolvedExportJob overwrite_job = make_job(root / "overwrite", chapters());
    std::filesystem::create_directories(overwrite_job.output_directory);
    std::ofstream {overwrite_job.segments.front().output_path} << "existing";
    auto overwrite_messages = std::vector<std::string> {};
    const ExportRunOptions overwrite_options {
        .message = [&overwrite_messages](const std::string& message) { overwrite_messages.push_back(message); },
    };
    const ExportRunResult overwritten = ExportEngine {successful_executor}.run({overwrite_job}, overwrite_options);
    test_support::expect_true(overwritten.ok(), "overwrite mode should export existing output");
    test_support::expect_true(
        overwritten.jobs.front().segments.front().overwrote_existing, "existing chapter should be marked overwritten");
    test_support::expect_true(!overwrite_messages.empty(), "overwrite mode should report existing output");

    auto continue_call = size_t {0};
    const auto continue_executor = [&continue_call](const ProcessRequest&) -> ProcessResult {
        ++continue_call;
        return continue_call == 2 ? ProcessResult {
                                        .state = ProcessExitState::NonzeroExit,
                                        .exit_code = 7,
                                        .standard_error = "bounded diagnostic",
                                    }
                                  : ProcessResult {.state = ProcessExitState::Success};
    };
    const ExportRunResult continued =
        ExportEngine {continue_executor}.run({make_job(root, chapters()), make_job(root / "second", chapters())});
    test_support::expect_eq(
        continued.exit_code, ExportExitCode::ExportFailure, "nonzero ffmpeg should map to exit code 2");
    test_support::expect_eq(
        continued.jobs.front().segments.size(), size_t {3}, "default policy should continue after a failed chapter");
    test_support::expect_eq(continued.jobs.size(), size_t {2}, "default policy should continue to later jobs");
    test_support::expect_true(!continued.jobs.front().segments[1].ok(), "failed chapter should remain explicit");
    test_support::expect_eq(
        continued.jobs.front().segments[1].process.exit_code, i32 {7}, "ffmpeg exit code should be retained");
    test_support::expect_eq(continued.jobs.front().segments[1].process.standard_error,
        std::string {"bounded diagnostic"},
        "bounded ffmpeg diagnostics should be retained");
    test_support::expect_true(
        continued.jobs.front().segments[1].process.error_message.find("exit code 7") != std::string::npos,
        "nonzero ffmpeg failure should include its exit code");
    test_support::expect_true(
        continued.jobs.front().segments[1].process.error_message.find(path_to_utf8(successful_job.metadata.source_path))
            != std::string::npos,
        "nonzero ffmpeg failure should include the source path");

    auto stop_call = size_t {0};
    const auto stop_executor = [&stop_call](const ProcessRequest&) -> ProcessResult {
        ++stop_call;
        return stop_call == 2 ? ProcessResult {.state = ProcessExitState::Crashed}
                              : ProcessResult {.state = ProcessExitState::Success};
    };
    ResolvedExportJob stop_job = make_job(root, chapters());
    stop_job.settings.stop_on_first_error = true;
    const ExportRunResult stopped = ExportEngine {stop_executor}.run({stop_job});
    test_support::expect_eq(
        stopped.exit_code, ExportExitCode::ExportFailure, "crashed ffmpeg should map to exit code 2");
    test_support::expect_true(stopped.stopped_early, "stop policy should report an early stop");
    test_support::expect_eq(stopped.jobs.front().segments.size(), size_t {2}, "stop policy should skip later chapters");

    auto invalid_plan_calls = size_t {0};
    const auto invalid_plan_executor = [&invalid_plan_calls](const ProcessRequest&) -> ProcessResult {
        ++invalid_plan_calls;
        return ProcessResult {.state = ProcessExitState::Success};
    };
    ResolvedExportJob invalid_plan_job = make_job(root, chapters());
    invalid_plan_job.segments.front().command.clear();
    const ExportRunResult invalid_plan = ExportEngine {invalid_plan_executor}.run({invalid_plan_job});
    test_support::expect_eq(
        invalid_plan.exit_code, ExportExitCode::ExportFailure, "invalid immutable plan should fail export");
    test_support::expect_eq(invalid_plan_calls, size_t {0}, "invalid immutable plan should not start a process");

    const auto missing_executor = [](const ProcessRequest&) -> ProcessResult {
        return ProcessResult {.state = ProcessExitState::FailedStart, .error_message = "not found"};
    };
    const ExportRunResult missing = ExportEngine {missing_executor}.run({make_job(root, chapters())});
    test_support::expect_eq(
        missing.exit_code, ExportExitCode::ToolingError, "missing ffmpeg should map to tooling exit code 3");
    test_support::expect_eq(
        missing.jobs.front().segments.size(), size_t {3}, "missing tool failures should remain explicit per chapter");
    test_support::expect_true(missing.jobs.front().segments.front().process.error_message.find(
                                  path_to_utf8(path_from_utf8(successful_job.segments.front().command.front())))
            != std::string::npos,
        "missing-tool failure should identify the executable path");
    test_support::expect_true(
        missing.jobs.front().segments.front().process.error_message.find("failed start") != std::string::npos,
        "missing-tool failure should not be reported as a timeout");

    ResolvedExportJob verified_job = make_job(root / "verified", {chapters().front()});
    verified_job.settings.verify_output_durations = true;
    auto verification_calls = size_t {0};
    auto progress = std::vector<int> {};
    auto process_output = std::string {};
    const auto verified_executor = [&verification_calls](const ProcessRequest& request) -> ProcessResult {
        ++verification_calls;
        if (request.executable.filename() == "ffprobe") {
            return ProcessResult {
                .state = ProcessExitState::Success,
                .standard_output =
                    R"({"format":{"duration":"2"},"streams":[{"codec_type":"video","avg_frame_rate":"30/1"}],"chapters":[]})",
            };
        }
        if (request.standard_output_chunk) {
            request.standard_output_chunk("out_time_ms=1000");
            request.standard_output_chunk("000\nprogress=continue\n");
        }
        if (request.standard_error_chunk) {
            request.standard_error_chunk("bounded raw output");
        }
        return ProcessResult {.state = ProcessExitState::Success};
    };
    const ExportRunResult verified = ExportEngine {verified_executor}.run({verified_job},
        ExportRunOptions {
            .progress_changed = [&progress](const int value) { progress.push_back(value); },
            .process_output = [&process_output](const std::string_view value) { process_output.append(value); },
        });
    test_support::expect_true(verified.ok(), "matching ffprobe duration should preserve export success");
    test_support::expect_eq(verification_calls, size_t {2}, "duration verification should run after ffmpeg");
    test_support::expect_true(
        verified.jobs.front().segments.front().duration_verified, "a matching duration should be explicit");
    test_support::expect_eq(
        verified.jobs.front().segments.front().actual_duration_ms, u64 {2000}, "verified duration should be retained");
    test_support::expect_true(progress.size() >= 3, "progress should include start, streamed progress, and completion");
    test_support::expect_eq(progress.front(), 0, "progress should start at zero");
    test_support::expect_eq(progress.back(), 100, "progress should finish at one hundred");
    test_support::expect_true(
        std::ranges::find(progress, 50) != progress.end(), "split ffmpeg progress chunks should parse incrementally");
    test_support::expect_eq(
        process_output, std::string {"bounded raw output"}, "bounded process output should reach adapters");

    const auto mismatched_executor = [](const ProcessRequest& request) -> ProcessResult {
        if (request.executable.filename() == "ffprobe") {
            return ProcessResult {
                .state = ProcessExitState::Success,
                .standard_output =
                    R"({"format":{"duration":"4"},"streams":[{"codec_type":"video","avg_frame_rate":"30/1"}],"chapters":[]})",
            };
        }
        return ProcessResult {.state = ProcessExitState::Success};
    };
    const ExportRunResult mismatched = ExportEngine {mismatched_executor}.run({verified_job});
    test_support::expect_true(!mismatched.ok(), "a mismatched rendered duration should fail its segment");
    test_support::expect_true(
        mismatched.jobs.front().segments.front().verification_error.find("expected 2000 ms, observed 4000 ms")
            != std::string::npos,
        "duration mismatch should retain expected and observed values");

    auto cancellation = std::stop_source {};
    auto cancellation_calls = size_t {0};
    const auto cancelling_executor = [&cancellation, &cancellation_calls](const ProcessRequest&) -> ProcessResult {
        ++cancellation_calls;
        cancellation.request_stop();
        return ProcessResult {
            .state = ProcessExitState::Cancelled,
            .error_message = "cancelled by test",
        };
    };
    const ExportRunResult cancelled = ExportEngine {cancelling_executor}.run(
        {make_job(root / "cancelled", chapters())}, ExportRunOptions {.stop_token = cancellation.get_token()});
    test_support::expect_true(cancelled.cancelled, "process cancellation should be explicit at batch level");
    test_support::expect_true(!cancelled.ok(), "cancelled export should fail");
    test_support::expect_eq(cancellation_calls, size_t {1}, "cancellation should prevent later chapters");
    test_support::expect_eq(cancelled.jobs.front().segments.front().process.state,
        ProcessExitState::Cancelled,
        "cancelled chapter should retain its process state");

    std::filesystem::remove_all(root);
    return 0;
}
