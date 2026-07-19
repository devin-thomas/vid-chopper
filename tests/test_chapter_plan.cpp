#include "core/chapter_plan.hpp"
#include "test_support.hpp"

#include <cstddef>

using namespace vidchopper;

auto main() -> int {
    const auto chapters = build_default_chapters(120000, 6);
    test_support::expect_eq(chapters.size(), static_cast<size_t>(6), "default chapter count should be six");
    test_support::expect_eq(chapters.front().start_ms, 0ULL, "first chapter should start at zero");
    test_support::expect_eq(chapters.back().end_ms, 120000ULL, "last chapter should end at source duration");

    auto settings = ExportSettings {};
    auto valid = validate_chapters(chapters, 120000, settings);
    test_support::expect_true(valid.ok(), "distributed chapters should validate");

    auto invalid_chapters = chapters;
    invalid_chapters[1].start_ms = invalid_chapters[0].end_ms - 500;
    invalid_chapters[1].end_ms = invalid_chapters[1].start_ms + 900;
    invalid_chapters[2].name.clear();

    const auto invalid = validate_chapters(invalid_chapters, 120000, settings);
    test_support::expect_true(!invalid.ok(), "invalid chapter plan should fail validation");
    test_support::expect_true(
        invalid.issues.size() >= 3, "validation should report overlap, short duration, and blank name");

    const auto compact_chapters = build_default_chapters(4500, 6);
    test_support::expect_eq(
        compact_chapters.size(), static_cast<size_t>(4), "short clips should not produce sub-second default chapters");

    return 0;
}
