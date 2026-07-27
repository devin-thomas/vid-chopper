#include "cli/export_runner.hpp"
#include "core/command_builder.hpp"
#include "test_support.hpp"

#include <chrono>
#include <filesystem>
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
    return ResolvedExportJob {
        .metadata =
            VideoMetadata {
                .source_path = root / "source clips" / "match.mkv",
                .duration_ms = 6000,
                .frame_rate = FrameRate {.numerator = 60, .denominator = 1},
                .source_extension = ".mkv",
            },
        .chapters = std::move(chapters),
        .output_directory = root / "rendered clips",
        .settings = settings,
        .environment = EncoderEnvironment {},
    };
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
    const std::vector<std::string> expected_command = build_ffmpeg_command(successful_job.metadata,
        successful_job.chapters.front(),
        first.output_path,
        successful_job.settings,
        successful_job.environment);
    test_support::expect_eq(
        observed_requests.front().executable, Path {expected_command.front()}, "runner should preserve executable");
    test_support::expect_eq(observed_requests.front().arguments,
        std::vector<std::string> {expected_command.begin() + 1, expected_command.end()},
        "runner should preserve command-builder token order");

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
