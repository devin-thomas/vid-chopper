#include "cli/output_planner.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <string>
#include <vector>

using namespace vidchopper;

namespace {

[[nodiscard]] auto make_input(const Path& source_path) -> OutputPlanInput {
    return OutputPlanInput {
        .metadata =
            VideoMetadata {
                .source_path = source_path,
                .duration_ms = 2000,
                .frame_rate = {.numerator = 30, .denominator = 1},
                .source_extension = ".MOV",
            },
        .chapters =
            {
                ChapterSegment {.name = "Opening", .start_ms = 0, .end_ms = 1000},
                ChapterSegment {
                    .name = "Display Name",
                    .start_ms = 1000,
                    .end_ms = 2000,
                    .output_name = "Final:<Cut>?",
                },
            },
    };
}

} // namespace

auto main() -> int {
    const OutputPlanResult defaults = plan_outputs({make_input("C:/media/Match One.MOV")});
    test_support::expect_true(defaults.ok(), "default output plan should succeed");
    test_support::expect_eq(defaults.jobs.front().output_directory.generic_string(),
        std::string {"C:/media/Match One_chapters"},
        "default output folder should match the GUI");
    test_support::expect_eq(defaults.jobs.front().segments.front().output_path.filename().string(),
        std::string {"01 - Opening.mov"},
        "default output naming should match the GUI");

    auto overridden = make_input("C:/media/Match One.MOV");
    overridden.settings.output_folder_pattern = "%source%_renders";
    overridden.settings.naming_pattern = "%index%_%name%";
    overridden.settings.index_padding = 3;

    const OutputPlanResult golden = plan_outputs({overridden});
    test_support::expect_true(golden.ok(), "valid output plan should succeed");
    test_support::expect_eq(golden.jobs.size(), size_t {1}, "one input should produce one immutable job");
    test_support::expect_eq(golden.jobs.front().output_directory.generic_string(),
        std::string {"C:/media/Match One_renders"},
        "ChapterFile output folder override should use the GUI-compatible source token");
    test_support::expect_eq(golden.jobs.front().segments[0].output_path.filename().string(),
        std::string {"001_Opening.mov"},
        "index padding and source extension should remain compatible");
    test_support::expect_eq(golden.jobs.front().segments[1].output_path.filename().string(),
        std::string {"002_Final__Cut__.mov"},
        "outputName and reserved filename characters should resolve before export");
    test_support::expect_eq(golden.jobs.front().segments[1].command.back(),
        golden.jobs.front().segments[1].output_path.string(),
        "planned command should retain the exact planned output path");

    auto duplicate_names = make_input("C:/media/duplicates.mp4");
    duplicate_names.settings.naming_pattern = "%name%";
    duplicate_names.chapters[1].name = duplicate_names.chapters[0].name;
    duplicate_names.chapters[1].output_name.clear();
    const OutputPlanResult duplicate_job = plan_outputs({duplicate_names});
    test_support::expect_true(!duplicate_job.ok(), "duplicate chapter outputs should fail the complete plan");
    test_support::expect_true(duplicate_job.jobs.empty(), "a colliding job should not expose a partial plan");
    test_support::expect_eq(duplicate_job.errors.size(), size_t {1}, "one duplicate should produce one diagnostic");

    auto first_batch_job = make_input("C:/batch/alpha.mp4");
    auto second_batch_job = make_input("C:/batch/beta.mp4");
    first_batch_job.settings.output_folder_pattern = "exports";
    second_batch_job.settings.output_folder_pattern = "exports";
    first_batch_job.settings.naming_pattern = "same";
    second_batch_job.settings.naming_pattern = "SAME";
    first_batch_job.chapters.resize(1);
    second_batch_job.chapters.resize(1);
    const OutputPlanResult duplicate_batch = plan_outputs({first_batch_job, second_batch_job});
    test_support::expect_true(!duplicate_batch.ok(), "Windows case-insensitive batch collisions should fail");
    test_support::expect_true(duplicate_batch.jobs.empty(), "a colliding batch should not expose partial jobs");
    test_support::expect_eq(duplicate_batch.errors.size(), size_t {1}, "batch collision should be reported once");

    return 0;
}
