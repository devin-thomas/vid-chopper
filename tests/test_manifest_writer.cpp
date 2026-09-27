#include "core/path_utils.hpp"
#include "services/export_planner.hpp"
#include "services/manifest_writer.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace vidchopper;

namespace {

[[nodiscard]] auto json_string_literal(const std::string_view value) -> std::string {
    auto escaped = std::string {"\""};
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '\"':
            escaped += "\\\"";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    escaped.push_back('\"');
    return escaped;
}

[[nodiscard]] auto make_job(const Path& root) -> ResolvedExportJob {
    auto settings = ExportSettings {};
    settings.output_folder_pattern = "output";
    settings.write_json_manifest = true;
    settings.write_csv_manifest = false;
    const auto input = OutputPlanInput {
        .metadata =
            VideoMetadata {
                .source_path = root / path_from_utf8("源 视频 🎬.mp4"),
                .duration_ms = 2000,
                .frame_rate = FrameRate {.numerator = 30, .denominator = 1},
                .source_extension = ".mp4",
            },
        .chapters = {{.name = "序章 🎬", .start_ms = 0, .end_ms = 2000}},
        .settings = settings,
    };
    OutputPlanResult plan = plan_outputs({input});
    if (!plan.ok()) {
        test_support::fail("manifest test job should plan successfully");
    }
    return std::move(plan.jobs.front());
}

[[nodiscard]] auto successful_run(const ResolvedExportJob& job) -> ExportRunResult {
    return ExportRunResult {
        .jobs = {ExportJobResult {
            .source_path = job.metadata.source_path,
            .segments = {RenderedSegment {
                .source_path = job.metadata.source_path,
                .chapter_name = job.segments.front().chapter.name,
                .output_path = job.segments.front().output_path,
                .process = ProcessResult {.state = ProcessExitState::Success},
            }},
        }},
    };
}

} // namespace

auto main() -> int {
    const Path root = std::filesystem::temp_directory_path() / "vidchopper-manifest-writer";
    auto cleanup_error = std::error_code {};
    std::filesystem::remove_all(root, cleanup_error);

    ResolvedExportJob job = make_job(root);
    const ExportRunResult run = successful_run(job);
    const ManifestWriteResult written = write_manifests({job}, run);
    test_support::expect_true(written.ok(), "complete manifest write should succeed");
    test_support::expect_eq(written.jobs.size(), size_t {1}, "manifest result should retain one job result");
    test_support::expect_true(written.jobs.front().ok(), "successful manifest job should be explicit");
    test_support::expect_true(std::filesystem::exists(job.output_directory / "vidchopper-manifest.json"),
        "successful manifest write should publish the final file");
    test_support::expect_true(!std::filesystem::exists(job.output_directory / "vidchopper-manifest.json.tmp"),
        "successful manifest write should not leave a temporary file");

    auto json_stream = std::ifstream {job.output_directory / "vidchopper-manifest.json", std::ios::binary};
    const std::string json_text {std::istreambuf_iterator<char> {json_stream}, std::istreambuf_iterator<char> {}};
    test_support::expect_true(
        json_text.find("\"source\": " + json_string_literal(path_to_utf8(job.metadata.source_path)))
            != std::string::npos,
        "JSON manifest paths should be serialized as UTF-8");
    test_support::expect_true(json_text.find(json_string_literal("序章 🎬")) != std::string::npos,
        "JSON manifest chapter text should preserve UTF-8");
    json_stream.close();

    const std::string banner_prefix = "ffmpeg version 8.0 Copyright";
    auto banner = std::string {banner_prefix};
    banner.append(size_t {4096}, 'x');
    ExportRunResult noisy_success = successful_run(job);
    noisy_success.jobs.front().segments.front().process.standard_error = banner;
    noisy_success.jobs.front().segments.front().duration_verified = true;
    const ManifestWriteResult noisy = write_manifests({job}, noisy_success);
    test_support::expect_true(noisy.ok(), "successful segment with ffmpeg stderr should still write");
    auto noisy_stream = std::ifstream {job.output_directory / "vidchopper-manifest.json", std::ios::binary};
    const std::string noisy_text {std::istreambuf_iterator<char> {noisy_stream}, std::istreambuf_iterator<char> {}};
    noisy_stream.close();
    const bool hides_success_banner = noisy_text.find(banner_prefix) == std::string::npos;
    test_support::expect_true(hides_success_banner, "successful segment error should not store the ffmpeg banner");
    const bool omits_success_error = noisy_text.find("\"error\"") == std::string::npos;
    test_support::expect_true(omits_success_error, "successful verified segment should omit error");
    const bool omits_success_tail = noisy_text.find("\"stderrTail\"") == std::string::npos;
    test_support::expect_true(omits_success_tail, "successful segment should omit stderrTail");

    auto failure_banner = std::string {"HEADMARKER"};
    failure_banner.append(size_t {2000}, 'y');
    failure_banner += "TAILMARKER";
    ExportRunResult failed_run = successful_run(job);
    RenderedSegment& failed_segment = failed_run.jobs.front().segments.front();
    failed_segment.process.state = ProcessExitState::NonzeroExit;
    failed_segment.process.exit_code = 1;
    failed_segment.process.error_message = "encoder failed";
    failed_segment.process.standard_error = failure_banner;
    const ManifestWriteResult failed = write_manifests({job}, failed_run);
    auto failed_stream = std::ifstream {job.output_directory / "vidchopper-manifest.json", std::ios::binary};
    const std::string failed_text {std::istreambuf_iterator<char> {failed_stream}, std::istreambuf_iterator<char> {}};
    failed_stream.close();
    const bool keeps_failure_error = failed_text.find("encoder failed") != std::string::npos;
    test_support::expect_true(keeps_failure_error, "failed segment should keep its error message");
    const bool keeps_tail = failed_text.find("TAILMARKER") != std::string::npos;
    test_support::expect_true(keeps_tail, "failed segment should keep a stderr tail");
    const bool drops_head = failed_text.find("HEADMARKER") == std::string::npos;
    test_support::expect_true(drops_head, "stderrTail should stay short");

    ResolvedExportJob blocked_job = make_job(root / "blocked");
    std::filesystem::create_directories(blocked_job.output_directory / "vidchopper-manifest.json.tmp");
    const ManifestWriteResult blocked = write_manifests({blocked_job}, successful_run(blocked_job));
    test_support::expect_true(!blocked.ok(), "unwritable manifest temporary path should fail the batch");
    test_support::expect_true(!blocked.jobs.front().ok(), "manifest failure should fail its owning job");
    test_support::expect_eq(blocked.preserved_media_paths,
        std::vector<Path> {blocked_job.segments.front().output_path},
        "manifest failure should list already-rendered media paths");
    test_support::expect_true(
        blocked.errors.front().find(path_to_utf8(blocked_job.metadata.source_path)) != std::string::npos,
        "manifest failure should identify its source job");

    std::filesystem::remove_all(root, cleanup_error);
    return 0;
}
