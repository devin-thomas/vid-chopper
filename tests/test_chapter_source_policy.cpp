#include "cli/chapter_source_policy.hpp"
#include "test_support.hpp"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace vidchopper;

namespace {

[[nodiscard]] auto contains(const std::string_view text, const std::string_view needle) -> bool {
    return text.find(needle) != std::string_view::npos;
}

[[nodiscard]] auto metadata(const Path& source_path, const bool has_embedded_chapters) -> VideoMetadata {
    auto chapters = std::vector<ChapterSegment> {};
    if (has_embedded_chapters) {
        chapters.push_back(ChapterSegment {.name = "Round 1", .start_ms = 0, .end_ms = 1000});
    }
    return VideoMetadata {.source_path = source_path, .embedded_chapters = std::move(chapters)};
}

} // namespace

auto main() -> int {
    const auto embedded = metadata(Path {R"(C:\match clips\round "one".mkv)"}, true);
    const std::string guidance = chapter_source_guidance(embedded);
    test_support::expect_true(
        contains(guidance, "Embedded chapters were found"), "embedded metadata should produce explicit rerun guidance");
    test_support::expect_true(contains(guidance, R"(VidChopperCLI.exe "C:\match clips\round \"one\".mkv" --embedded)"),
        "rerun guidance should preserve spaces and quotes using Windows argument escaping");

    const auto without_embedded = metadata(Path {"plain.mkv"}, false);
    const std::string missing_guidance = chapter_source_guidance(without_embedded);
    test_support::expect_true(contains(missing_guidance, "JSON or YAML chapter config is required"),
        "missing embedded metadata should require a chapter file");
    test_support::expect_true(!contains(missing_guidance, "--embedded"),
        "missing embedded metadata should not recommend an unusable command");

    const VideoMetadataList mixed_batch = {
        metadata(Path {"with-chapters.mkv"}, true),
        metadata(Path {"without-chapters.mkv"}, false),
    };
    const EmbeddedChapterSelection selection = select_embedded_sources(mixed_batch);
    test_support::expect_eq(selection.selected.size(), size_t {1}, "mixed batch should select embedded sources");
    test_support::expect_eq(selection.skipped.size(), size_t {1}, "mixed batch should report missing sources");
    test_support::expect_eq(selection.skipped.front(),
        Path {"without-chapters.mkv"},
        "mixed batch should preserve the skipped source path");

    return 0;
}
