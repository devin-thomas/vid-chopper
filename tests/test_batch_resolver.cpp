#include "cli/batch_resolver.hpp"
#include "core/path_utils.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

using namespace vidchopper;

namespace {

class TemporaryDirectory final {
public:
    explicit TemporaryDirectory(Path path)
        : path_ {std::move(path)} {
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        auto error = std::error_code {};
        std::filesystem::remove_all(path_, error);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    auto operator=(const TemporaryDirectory&) -> TemporaryDirectory& = delete;

    [[nodiscard]] auto path() const -> const Path& {
        return path_;
    }

private:
    Path path_;
};

auto touch(const Path& path) -> void {
    std::filesystem::create_directories(path.parent_path());
    auto output = std::ofstream {path};
    output << "fixture\n";
}

[[nodiscard]] auto contains(const std::string_view text, const std::string_view needle) -> bool {
    return text.find(needle) != std::string_view::npos;
}

[[nodiscard]] auto joined_errors(const BatchResolution& result) -> std::string {
    auto joined = std::string {};
    for (const std::string& error : result.errors) {
        joined += error;
        joined.push_back('\n');
    }
    return joined;
}

} // namespace

auto main() -> int {
    const auto root = TemporaryDirectory {
        std::filesystem::temp_directory_path() / "vidchopper-batch-resolver",
    };

    const Path one_source = root.path() / "one" / "Match.MKV";
    const Path one_config = root.path() / "one" / "chapters.yaml";
    touch(one_source);
    touch(one_config);

    const BatchResolution one_to_one = resolve_batch(BatchResolveRequest {
        .source_path = one_source,
        .chapter_source_path = one_config,
    });
    test_support::expect_true(one_to_one.ok(), "one source and one config should resolve");
    test_support::expect_eq(one_to_one.jobs.size(), size_t {1}, "1:1 should produce one job");
    test_support::expect_eq(one_to_one.jobs.front().source_path, one_source, "1:1 should preserve source casing");
    test_support::expect_eq(
        one_to_one.jobs.front().chapter_config_path, std::optional<Path> {one_config}, "1:1 should use config");

    const Path shared_sources = root.path() / "shared" / "videos";
    const Path shared_config = root.path() / "shared" / "shared.json";
    const Path shared_a = shared_sources / "alpha.mp4";
    const Path shared_b = shared_sources / "Beta.mov";
    touch(shared_b);
    touch(shared_a);
    touch(shared_config);

    const BatchResolution many_to_one = resolve_batch(BatchResolveRequest {
        .source_path = shared_sources,
        .chapter_source_path = shared_config,
    });
    test_support::expect_true(many_to_one.ok(), "source directory and one config should resolve");
    test_support::expect_eq(many_to_one.jobs.size(), size_t {2}, "N:1 should produce one job per source");
    test_support::expect_eq(many_to_one.jobs[0].source_path, shared_a, "N:1 jobs should be deterministic");
    test_support::expect_eq(many_to_one.jobs[1].source_path, shared_b, "N:1 should preserve source casing");
    test_support::expect_eq(
        many_to_one.jobs[0].chapter_config_path, std::optional<Path> {shared_config}, "N:1 should share the config");

    const Path matched_sources = root.path() / "matched" / "videos";
    const Path matched_configs = root.path() / "matched" / "configs";
    const Path matched_fight = matched_sources / "Fight.MP4";
    const Path matched_other = matched_sources / "other.mov";
    const Path matched_fight_config = matched_configs / "fight.yaml";
    const Path matched_other_config = matched_configs / "OTHER.YML";
    touch(matched_other);
    touch(matched_fight);
    touch(matched_other_config);
    touch(matched_fight_config);

    const BatchResolution matched = resolve_batch(BatchResolveRequest {
        .source_path = matched_sources,
        .chapter_source_path = matched_configs,
    });
    test_support::expect_true(matched.ok(), "case-insensitive Windows stems should match");
    test_support::expect_eq(matched.jobs.size(), size_t {2}, "N:N should produce all jobs");
    test_support::expect_eq(matched.jobs[0].source_path, matched_fight, "N:N should sort by source stem");
    test_support::expect_eq(matched.jobs[0].chapter_config_path,
        std::optional<Path> {matched_fight_config},
        "N:N should retain actual config casing");

#ifdef _WIN32
    const Path unicode_sources = root.path() / "unicode" / "videos";
    const Path unicode_configs = root.path() / "unicode" / "configs";
    const Path unicode_eclair = unicode_sources / L"Éclair.MP4";
    const Path unicode_ecole = unicode_sources / L"éCOLE.mov";
    const Path unicode_eclair_config = unicode_configs / L"éCLAIR.json";
    const Path unicode_ecole_config = unicode_configs / L"École.yaml";
    touch(unicode_ecole);
    touch(unicode_eclair);
    touch(unicode_ecole_config);
    touch(unicode_eclair_config);
    const BatchResolution unicode_matched = resolve_batch(BatchResolveRequest {
        .source_path = unicode_sources,
        .chapter_source_path = unicode_configs,
    });
    test_support::expect_true(unicode_matched.ok(), "Windows ordinal matching should pair non-ASCII case variants");
    test_support::expect_eq(
        unicode_matched.jobs[0].source_path, unicode_eclair, "Windows ordinal ordering should be deterministic");
    test_support::expect_eq(unicode_matched.jobs[0].chapter_config_path,
        std::optional<Path> {unicode_eclair_config},
        "Windows ordinal matching should preserve original path casing");

    const Path unicode_duplicates = root.path() / "unicode-duplicates";
    const Path unicode_duplicate_upper = unicode_duplicates / L"Ångström.mp4";
    const Path unicode_duplicate_lower = unicode_duplicates / L"ångström.mkv";
    touch(unicode_duplicate_upper);
    touch(unicode_duplicate_lower);
    const BatchResolution unicode_duplicate_result = resolve_batch(BatchResolveRequest {
        .source_path = unicode_duplicates,
        .chapter_source_path = shared_config,
    });
    test_support::expect_true(
        !unicode_duplicate_result.ok(), "Windows ordinal duplicate detection should fold non-ASCII case variants");
    test_support::expect_true(contains(joined_errors(unicode_duplicate_result), path_to_utf8(unicode_duplicate_upper)),
        "non-ASCII duplicate diagnostics should preserve uppercase source spelling");
    test_support::expect_true(contains(joined_errors(unicode_duplicate_result), path_to_utf8(unicode_duplicate_lower)),
        "non-ASCII duplicate diagnostics should preserve lowercase source spelling");
#endif

    const Path one_to_many_configs = root.path() / "one-to-many" / "configs";
    touch(one_to_many_configs / "first.json");
    const BatchResolution one_to_many = resolve_batch(BatchResolveRequest {
        .source_path = one_source,
        .chapter_source_path = one_to_many_configs,
    });
    test_support::expect_true(!one_to_many.ok(), "one source and a config directory should fail");
    test_support::expect_true(contains(joined_errors(one_to_many), "1:N"), "1:N error should name the mode");
    test_support::expect_true(one_to_many.jobs.empty(), "invalid 1:N should not return partial jobs");

    const Path duplicate_sources = root.path() / "duplicate-sources";
    const Path duplicate_source_lower = duplicate_sources / "fight.mp4";
    const Path duplicate_source_upper = duplicate_sources / "FIGHT.mkv";
    touch(duplicate_source_lower);
    touch(duplicate_source_upper);
    const BatchResolution duplicate_source_result = resolve_batch(BatchResolveRequest {
        .source_path = duplicate_sources,
        .chapter_source_path = shared_config,
    });
    const std::string duplicate_source_errors = joined_errors(duplicate_source_result);
    test_support::expect_true(!duplicate_source_result.ok(), "duplicate source stems should fail");
    test_support::expect_true(contains(duplicate_source_errors, path_to_utf8(duplicate_source_lower)),
        "duplicate source error should include the first conflicting path");
    test_support::expect_true(contains(duplicate_source_errors, path_to_utf8(duplicate_source_upper)),
        "duplicate source error should include the second conflicting path");

    const Path duplicate_config_sources = root.path() / "duplicate-configs" / "videos";
    const Path duplicate_configs = root.path() / "duplicate-configs" / "configs";
    touch(duplicate_config_sources / "fight.mp4");
    const Path duplicate_config_json = duplicate_configs / "fight.json";
    const Path duplicate_config_yaml = duplicate_configs / "FIGHT.yaml";
    touch(duplicate_config_json);
    touch(duplicate_config_yaml);
    const BatchResolution duplicate_config_result = resolve_batch(BatchResolveRequest {
        .source_path = duplicate_config_sources,
        .chapter_source_path = duplicate_configs,
    });
    const std::string duplicate_config_errors = joined_errors(duplicate_config_result);
    test_support::expect_true(!duplicate_config_result.ok(), "duplicate config stems should fail");
    test_support::expect_true(contains(duplicate_config_errors, path_to_utf8(duplicate_config_json)),
        "duplicate config error should include the first conflicting path");
    test_support::expect_true(contains(duplicate_config_errors, path_to_utf8(duplicate_config_yaml)),
        "duplicate config error should include the second conflicting path");

    const Path mismatched_sources = root.path() / "mismatch" / "videos";
    const Path mismatched_configs = root.path() / "mismatch" / "configs";
    touch(mismatched_sources / "alpha.mp4");
    touch(mismatched_sources / "beta.mkv");
    touch(mismatched_configs / "alpha.json");
    touch(mismatched_configs / "gamma.yaml");
    const BatchResolution mismatch = resolve_batch(BatchResolveRequest {
        .source_path = mismatched_sources,
        .chapter_source_path = mismatched_configs,
    });
    const std::string mismatch_errors = joined_errors(mismatch);
    const auto expected_mismatch_errors = std::vector<std::string> {
        "Missing ChapterFile for Source: " + path_to_utf8(mismatched_sources / "beta.mkv") + ".",
        "Orphan ChapterFile: " + path_to_utf8(mismatched_configs / "gamma.yaml") + ".",
    };
    test_support::expect_true(!mismatch.ok(), "missing and orphan configs should fail");
    test_support::expect_eq(
        mismatch.errors, expected_mismatch_errors, "N:N diagnostics should be complete and deterministic");
    test_support::expect_true(contains(mismatch_errors, "Missing ChapterFile"), "mismatch should list missing configs");
    test_support::expect_true(contains(mismatch_errors, path_to_utf8(mismatched_sources / "beta.mkv")),
        "missing config diagnostic should include the source path");
    test_support::expect_true(contains(mismatch_errors, "Orphan ChapterFile"), "mismatch should list orphan configs");
    test_support::expect_true(contains(mismatch_errors, path_to_utf8(mismatched_configs / "gamma.yaml")),
        "orphan diagnostic should include the config path");
    test_support::expect_true(mismatch.jobs.empty(), "invalid N:N should not return partial jobs");

    const Path count_sources = root.path() / "count-mismatch" / "videos";
    const Path count_configs = root.path() / "count-mismatch" / "configs";
    touch(count_sources / "alpha.mp4");
    touch(count_sources / "beta.mkv");
    touch(count_configs / "alpha.json");
    const BatchResolution count_mismatch = resolve_batch(BatchResolveRequest {
        .source_path = count_sources,
        .chapter_source_path = count_configs,
    });
    test_support::expect_true(
        contains(joined_errors(count_mismatch), "count mismatch"), "unequal N:N counts should be explicit");

    const Path unsupported_source_path = root.path() / "unsupported" / "match.avi";
    touch(unsupported_source_path);
    const BatchResolution unsupported_source = resolve_batch(BatchResolveRequest {
        .source_path = unsupported_source_path,
        .chapter_source_path = shared_config,
    });
    test_support::expect_true(!unsupported_source.ok(), "unsupported source extensions should fail");
    test_support::expect_true(contains(joined_errors(unsupported_source), "match.avi"),
        "unsupported source error should preserve the filename");

    const Path unsupported_config = root.path() / "unsupported" / "chapters.txt";
    touch(unsupported_config);
    const BatchResolution unsupported_chapter_file = resolve_batch(BatchResolveRequest {
        .source_path = one_source,
        .chapter_source_path = unsupported_config,
    });
    test_support::expect_true(!unsupported_chapter_file.ok(), "unsupported config extensions should fail");
    test_support::expect_true(contains(joined_errors(unsupported_chapter_file), "chapters.txt"),
        "unsupported config error should preserve the filename");

    const BatchResolution both_unsupported = resolve_batch(BatchResolveRequest {
        .source_path = unsupported_source_path,
        .chapter_source_path = unsupported_config,
    });
    const auto expected_unsupported_errors = std::vector<std::string> {
        "Unsupported Source extension: " + path_to_utf8(unsupported_source_path)
            + ". Supported extensions: .mp4, .mkv, .mov.",
        "Unsupported ChapterFile extension: " + path_to_utf8(unsupported_config)
            + ". Supported extensions: .json, .yaml, .yml.",
    };
    test_support::expect_eq(both_unsupported.errors,
        expected_unsupported_errors,
        "independent direct-input errors should form one complete report");
    test_support::expect_true(both_unsupported.jobs.empty(), "a complete validation report should not include jobs");

    const Path empty_sources = root.path() / "empty" / "videos";
    const Path empty_configs = root.path() / "empty" / "configs";
    std::filesystem::create_directories(empty_sources);
    std::filesystem::create_directories(empty_configs);
    const BatchResolution empty_source_result = resolve_batch(BatchResolveRequest {
        .source_path = empty_sources,
        .chapter_source_path = shared_config,
    });
    test_support::expect_true(!empty_source_result.ok(), "empty source directories should fail");
    test_support::expect_true(contains(joined_errors(empty_source_result), path_to_utf8(empty_sources)),
        "empty source error should include the directory");

    const BatchResolution empty_config_result = resolve_batch(BatchResolveRequest {
        .source_path = matched_sources,
        .chapter_source_path = empty_configs,
    });
    test_support::expect_true(!empty_config_result.ok(), "empty config directories should fail");
    test_support::expect_true(contains(joined_errors(empty_config_result), path_to_utf8(empty_configs)),
        "empty config error should include the directory");

    const Path incomplete_sources = root.path() / "incomplete" / "videos";
    const Path incomplete_configs = root.path() / "incomplete" / "configs";
    std::filesystem::create_directories(incomplete_sources);
    std::filesystem::create_directories(incomplete_configs);
    const DirectoryScanner incomplete_scanner = [incomplete_sources, incomplete_configs](
                                                    const Path& directory) -> DirectoryScanResult {
        if (directory == incomplete_sources) {
            return DirectoryScanResult {
                .regular_files = {directory / "partial.mp4", directory / "PARTIAL.mkv"},
                .failures = {{.entry_path = directory / "zeta.mp4", .message = "zeta failure"},
                    {.entry_path = directory / "alpha.mp4", .message = "alpha failure"}},
                .complete = false,
            };
        }
        test_support::expect_eq(directory, incomplete_configs, "scanner should receive the config directory");
        return DirectoryScanResult {
            .regular_files = {directory / "orphan.yaml"},
            .failures = {{.message = "iterator failure"}},
            .complete = false,
        };
    };
    const BatchResolution incomplete = resolve_batch(BatchResolveRequest {
        .source_path = incomplete_sources,
        .chapter_source_path = incomplete_configs,
        .directory_scanner = incomplete_scanner,
    });
    const BatchResolution incomplete_again = resolve_batch(BatchResolveRequest {
        .source_path = incomplete_sources,
        .chapter_source_path = incomplete_configs,
        .directory_scanner = incomplete_scanner,
    });
    test_support::expect_true(!incomplete.ok(), "incomplete directory inventory should fail validation");
    test_support::expect_true(incomplete.jobs.empty(), "incomplete inventory must not produce partial jobs");
    test_support::expect_eq(
        incomplete.errors, incomplete_again.errors, "incomplete scan diagnostics should be byte-stable");
    const std::string incomplete_errors = joined_errors(incomplete);
    test_support::expect_true(
        !contains(incomplete_errors, "Duplicate"), "incomplete inventory must not derive duplicate conclusions");
    test_support::expect_true(!contains(incomplete_errors, "Missing ChapterFile"),
        "incomplete inventory must not derive missing-file conclusions");
    test_support::expect_true(
        !contains(incomplete_errors, "Orphan ChapterFile"), "incomplete inventory must not derive orphan conclusions");
    test_support::expect_true(
        !contains(incomplete_errors, "count mismatch"), "incomplete inventory must not derive count conclusions");

    const BatchResolution embedded = resolve_batch(BatchResolveRequest {
        .source_path = shared_sources,
        .use_embedded_chapters = true,
    });
    test_support::expect_true(embedded.ok(), "embedded directory mode should resolve supported sources");
    test_support::expect_eq(embedded.jobs.size(), size_t {2}, "embedded directory mode should create all jobs");
    test_support::expect_true(
        !embedded.jobs.front().chapter_config_path.has_value(), "embedded jobs should not have config paths");

    const BatchResolution embedded_file = resolve_batch(BatchResolveRequest {
        .source_path = one_source,
        .use_embedded_chapters = true,
    });
    test_support::expect_true(embedded_file.ok(), "single-file embedded mode should resolve");
    test_support::expect_eq(embedded_file.jobs.size(), size_t {1}, "single-file embedded mode should create one job");

    return 0;
}
