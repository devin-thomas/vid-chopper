#include "core/chapter_plan.h"
#include "test_support.h"

#include <cstddef>

using namespace vidchopper;

auto main() -> int {
    // build_default_chapters: zero duration
    {
        const auto chapters = build_default_chapters(0, 6);
        test_support::expect_eq(chapters.size(), std::size_t {0}, "zero duration should produce no chapters");
    }

    // build_default_chapters: sub-second duration
    {
        const auto chapters = build_default_chapters(500, 6);
        test_support::expect_eq(chapters.size(), std::size_t {0}, "sub-second duration should produce no chapters");
    }

    // build_default_chapters: exactly 1 second
    {
        const auto chapters = build_default_chapters(1000, 6);
        const auto expected_chapter_count = std::size_t {1};
        test_support::expect_eq(chapters.size(), expected_chapter_count, "1 second should produce 1 chapter");
        test_support::expect_eq(chapters[0].start_ms, 0ULL, "single chapter should start at 0");
        test_support::expect_eq(chapters[0].end_ms, 1000ULL, "single chapter should end at duration");
    }

    // build_default_chapters: requested_count = 0
    {
        const auto chapters = build_default_chapters(60000, 0);
        test_support::expect_eq(chapters.size(), std::size_t {0}, "zero requested count should produce no chapters");
    }

    // build_default_chapters: requested_count = 1
    {
        const auto chapters = build_default_chapters(60000, 1);
        test_support::expect_eq(chapters.size(), std::size_t {1}, "requesting 1 chapter should produce exactly 1");
        test_support::expect_eq(chapters[0].start_ms, 0ULL, "single chapter start");
        test_support::expect_eq(chapters[0].end_ms, 60000ULL, "single chapter end");
    }

    // build_default_chapters: chapters cover full duration without gaps
    {
        const auto chapters = build_default_chapters(100000, 10);
        test_support::expect_eq(chapters.size(), std::size_t {10}, "should produce requested number of chapters");
        test_support::expect_eq(chapters.front().start_ms, 0ULL, "first chapter starts at 0");
        test_support::expect_eq(chapters.back().end_ms, 100000ULL, "last chapter ends at duration");

        for (auto i = std::size_t {1}; i < chapters.size(); ++i) {
            test_support::expect_eq(chapters[i].start_ms, chapters[i - 1].end_ms, "chapters should be contiguous");
        }
    }

    // build_default_chapters: chapter names are sequential
    {
        const auto chapters = build_default_chapters(30000, 3);
        test_support::expect_eq(chapters[0].name, std::string {"Chapter 1"}, "first chapter name");
        test_support::expect_eq(chapters[1].name, std::string {"Chapter 2"}, "second chapter name");
        test_support::expect_eq(chapters[2].name, std::string {"Chapter 3"}, "third chapter name");
    }

    // validate_chapters: empty chapter list
    {
        auto settings = ExportSettings {};
        const auto result = validate_chapters({}, 60000, settings);
        test_support::expect_true(!result.ok(), "empty chapters should fail validation");
        test_support::expect_true(
            result.issues[0].message.find("At least one") != std::string::npos, "should report empty chapter error");
    }

    // validate_chapters: exceeds max_chapters
    {
        auto chapters = std::vector<ChapterSegment> {};
        for (auto i = 0; i < 260; ++i) {
            chapters.push_back(ChapterSegment {
                .name = "Ch " + std::to_string(i + 1),
                .start_ms = static_cast<u64>(i) * 1000,
                .end_ms = static_cast<u64>(i + 1) * 1000,
            });
        }
        auto settings = ExportSettings {};
        const auto result = validate_chapters(chapters, 260000, settings);
        test_support::expect_true(!result.ok(), "exceeding max chapters should fail");
        test_support::expect_true(
            result.issues[0].message.find("255") != std::string::npos, "should mention 255 chapter limit");
    }

    // validate_chapters: chapter end exceeds duration
    {
        auto chapters = std::vector<ChapterSegment> {
            {.name = "Over", .start_ms = 0, .end_ms = 70000},
        };
        auto settings = ExportSettings {};
        const auto result = validate_chapters(chapters, 60000, settings);
        test_support::expect_true(!result.ok(), "chapter exceeding duration should fail");
    }

    // validate_chapters: chapter with end <= start
    {
        auto chapters = std::vector<ChapterSegment> {
            {.name = "Backwards", .start_ms = 5000, .end_ms = 5000},
        };
        auto settings = ExportSettings {};
        const auto result = validate_chapters(chapters, 60000, settings);
        test_support::expect_true(!result.ok(), "chapter with end == start should fail");
    }

    // validate_chapters: single valid chapter
    {
        auto chapters = std::vector<ChapterSegment> {
            {.name = "Solo", .start_ms = 0, .end_ms = 30000},
        };
        auto settings = ExportSettings {};
        const auto result = validate_chapters(chapters, 30000, settings);
        test_support::expect_true(result.ok(), "single valid chapter should pass");
    }

    // validate_chapters: chapter too short (below min_chapter_seconds)
    {
        auto chapters = std::vector<ChapterSegment> {
            {.name = "Short", .start_ms = 0, .end_ms = 500},
        };
        auto settings = ExportSettings {};
        settings.min_chapter_seconds = 1;
        const auto result = validate_chapters(chapters, 60000, settings);
        test_support::expect_true(!result.ok(), "chapter shorter than min duration should fail");
    }

    // validate_chapters: overlapping chapters
    {
        auto chapters = std::vector<ChapterSegment> {
            {.name = "First", .start_ms = 0, .end_ms = 10000},
            {.name = "Second", .start_ms = 8000, .end_ms = 20000},
        };
        auto settings = ExportSettings {};
        const auto result = validate_chapters(chapters, 20000, settings);
        test_support::expect_true(!result.ok(), "overlapping chapters should fail");
    }

    // default_output_directory: basic pattern substitution
    {
        auto settings = ExportSettings {};
        settings.output_folder_pattern = "%source%_chapters";
        const auto dir = default_output_directory("C:/media/my-video.mp4", settings);
        test_support::expect_true(dir.filename().string().find("my-video_chapters") != std::string::npos,
            "should substitute source stem into folder pattern");
    }

    // default_output_directory: custom pattern
    {
        auto settings = ExportSettings {};
        settings.output_folder_pattern = "export_%source%";
        const auto dir = default_output_directory("/home/user/clip.mov", settings);
        test_support::expect_true(dir.filename().string().find("export_clip") != std::string::npos,
            "custom pattern should substitute source stem");
    }

    // default_output_directory: pattern without placeholder
    {
        auto settings = ExportSettings {};
        settings.output_folder_pattern = "output_clips";
        const auto dir = default_output_directory("/media/vid.mp4", settings);
        test_support::expect_true(dir.filename().string().find("output_clips") != std::string::npos,
            "pattern without placeholder should use literal name");
    }

    // ValidationResult::ok()
    {
        auto empty_result = ValidationResult {};
        test_support::expect_true(empty_result.ok(), "empty issues means ok");

        auto has_issues = ValidationResult {};
        has_issues.issues.push_back(ValidationIssue {.message = "problem"});
        test_support::expect_true(!has_issues.ok(), "non-empty issues means not ok");
    }

    return 0;
}
