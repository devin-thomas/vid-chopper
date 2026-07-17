#include "core/chapter_plan.hpp"
#include "core/models.hpp"
#include "test_support.hpp"

#include <vector>

using namespace vidchopper;

// Golden tests for the chapter-planning core. They lock the exact segments
// build_default_chapters emits and the exact issue list validate_chapters
// produces, exercising the defaulted ChapterSegment / ValidationIssue
// comparisons added in the const-correctness pass.
auto main() -> int {
    // build_default_chapters: even split with an exact final boundary.
    {
        const auto chapters = build_default_chapters(10000, 4);
        const auto expected = std::vector<ChapterSegment> {
            {.name = "Chapter 1", .start_ms = 0, .end_ms = 2500},
            {.name = "Chapter 2", .start_ms = 2500, .end_ms = 5000},
            {.name = "Chapter 3", .start_ms = 5000, .end_ms = 7500},
            {.name = "Chapter 4", .start_ms = 7500, .end_ms = 10000},
        };
        test_support::expect_eq(chapters, expected, "even four-way split");
    }

    // build_default_chapters: count is clamped to one chapter per whole second.
    {
        const auto chapters = build_default_chapters(2500, 5);
        const auto expected = std::vector<ChapterSegment> {
            {.name = "Chapter 1", .start_ms = 0, .end_ms = 1250},
            {.name = "Chapter 2", .start_ms = 1250, .end_ms = 2500},
        };
        test_support::expect_eq(chapters, expected, "count clamped to whole seconds");
    }

    // build_default_chapters: degenerate inputs yield no chapters.
    {
        test_support::expect_true(build_default_chapters(800, 4).empty(), "sub-second duration yields none");
        test_support::expect_true(build_default_chapters(5000, 0).empty(), "zero requested yields none");
    }

    // validate_chapters: empty plan reports the single required-chapter issue.
    {
        const auto result = validate_chapters({}, 20000, ExportSettings {});
        const auto expected = std::vector<ValidationIssue> {
            {.chapter_index = 0, .message = "At least one chapter is required."},
        };
        test_support::expect_true(!result.ok(), "empty plan is not ok");
        test_support::expect_eq(result.issues, expected, "empty plan issue list");
    }

    // validate_chapters: per-chapter issues fire in source order with indices.
    {
        const auto chapters = std::vector<ChapterSegment> {
            {.name = "Intro", .start_ms = 0, .end_ms = 5000},
            {.name = "Bad", .start_ms = 5000, .end_ms = 5000},
            {.name = "", .start_ms = 6000, .end_ms = 12000},
        };
        const auto result = validate_chapters(chapters, 20000, ExportSettings {});
        const auto expected = std::vector<ValidationIssue> {
            {.chapter_index = 1, .message = "Chapter end time must be after the start time."},
            {.chapter_index = 2, .message = "Chapter names cannot be blank."},
        };
        test_support::expect_eq(result.issues, expected, "ordered per-chapter issues");
    }

    // validate_chapters: a clean plan produces no issues.
    {
        const auto chapters = std::vector<ChapterSegment> {
            {.name = "Intro", .start_ms = 0, .end_ms = 5000},
            {.name = "Body", .start_ms = 5000, .end_ms = 15000},
        };
        const auto result = validate_chapters(chapters, 20000, ExportSettings {});
        test_support::expect_true(result.ok(), "clean plan is ok");
    }

    return 0;
}
