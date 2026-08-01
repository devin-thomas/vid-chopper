#include "cli/export_runner.hpp"
#include "cli/output_planner.hpp"
#include "test_support.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

using namespace vidchopper;

namespace {

[[nodiscard]] auto make_job(const Path& root, std::vector<ChapterSegment> chapters) -> ResolvedExportJob {
    auto settings = ExportSettings {};
    settings.ffmpeg_path = R"(C:\Program Files\ffmpeg\ffmpeg.exe)";
    settings.overwrite_mode = OverwriteMode::Overwrite;
    settings.container_mode = ContainerMode::Mp4;
    settings.stop_on_first_error = false;
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
    const ExportRunResult successful = ExportRunner {successful_executor}.run({successful_job}, capture_options);
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
    test_support::expect_eq(
        observed_requests.front().executable, Path {expected_command.front()}, "runner should preserve executable");
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
    const ExportRunResult skipped = ExportRunner {skip_executor}.run({skip_job});
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
    const ExportRunResult overwritten = ExportRunner {successful_executor}.run({overwrite_job}, overwrite_options);
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
        ExportRunner {continue_executor}.run({make_job(root, chapters()), make_job(root / "second", chapters())});
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

    auto stop_call = size_t {0};
    const auto stop_executor = [&stop_call](const ProcessRequest&) -> ProcessResult {
        ++stop_call;
        return stop_call == 2 ? ProcessResult {.state = ProcessExitState::Crashed}
                              : ProcessResult {.state = ProcessExitState::Success};
    };
    ResolvedExportJob stop_job = make_job(root, chapters());
    stop_job.settings.stop_on_first_error = true;
    const ExportRunResult stopped = ExportRunner {stop_executor}.run({stop_job});
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
    const ExportRunResult invalid_plan = ExportRunner {invalid_plan_executor}.run({invalid_plan_job});
    test_support::expect_eq(
        invalid_plan.exit_code, ExportExitCode::ExportFailure, "invalid immutable plan should fail export");
    test_support::expect_eq(invalid_plan_calls, size_t {0}, "invalid immutable plan should not start a process");

    const auto missing_executor = [](const ProcessRequest&) -> ProcessResult {
        return ProcessResult {.state = ProcessExitState::FailedStart, .error_message = "not found"};
    };
    const ExportRunResult missing = ExportRunner {missing_executor}.run({make_job(root, chapters())});
    test_support::expect_eq(
        missing.exit_code, ExportExitCode::ToolingError, "missing ffmpeg should map to tooling exit code 3");
    test_support::expect_eq(
        missing.jobs.front().segments.size(), size_t {3}, "missing tool failures should remain explicit per chapter");

    std::filesystem::remove_all(root);
    return 0;
}
