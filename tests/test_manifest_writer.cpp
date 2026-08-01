#include "cli/manifest_writer.hpp"
#include "cli/output_planner.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

using namespace vidchopper;

namespace {

[[nodiscard]] auto make_job(const Path& root) -> ResolvedExportJob {
    auto settings = ExportSettings {};
    settings.output_folder_pattern = "output";
    settings.write_json_manifest = true;
    settings.write_csv_manifest = false;
    const auto input = OutputPlanInput {
        .metadata =
            VideoMetadata {
                .source_path = root / "source.mp4",
                .duration_ms = 2000,
                .frame_rate = FrameRate {.numerator = 30, .denominator = 1},
                .source_extension = ".mp4",
            },
        .chapters = {{.name = "Match", .start_ms = 0, .end_ms = 2000}},
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

    ResolvedExportJob blocked_job = make_job(root / "blocked");
    std::filesystem::create_directories(blocked_job.output_directory / "vidchopper-manifest.json.tmp");
    const ManifestWriteResult blocked = write_manifests({blocked_job}, successful_run(blocked_job));
    test_support::expect_true(!blocked.ok(), "unwritable manifest temporary path should fail the batch");
    test_support::expect_true(!blocked.jobs.front().ok(), "manifest failure should fail its owning job");
    test_support::expect_eq(blocked.preserved_media_paths,
        std::vector<Path> {blocked_job.segments.front().output_path},
        "manifest failure should list already-rendered media paths");
    test_support::expect_true(
        blocked.errors.front().find(blocked_job.metadata.source_path.string()) != std::string::npos,
        "manifest failure should identify its source job");

    std::filesystem::remove_all(root, cleanup_error);
    return 0;
}
