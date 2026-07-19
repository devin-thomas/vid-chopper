#include "cli/chapter_config.hpp"

#include "test_support.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

using namespace vidchopper;

namespace {

auto write_text(const Path& path, const std::string_view text) -> void {
    std::filesystem::create_directories(path.parent_path());
    auto stream = std::ofstream {path};
    stream << text;
}

[[nodiscard]] auto contains(const std::string& text, const std::string_view needle) -> bool {
    return text.find(needle) != std::string::npos;
}

auto require_success(const ChapterConfigLoadResult& result, const std::string_view message) -> void {
    if (!result.ok()) {
        test_support::fail(std::string {message} + ": " + result.error_message);
    }
}

auto require_failure(
    const ChapterConfigLoadResult& result, const std::string_view message, const std::string_view detail) -> void {
    test_support::expect_true(!result.ok(), message);
    test_support::expect_true(contains(result.error_message, detail), message);
}

} // namespace

auto main() -> int {
    const Path root = std::filesystem::temp_directory_path() / "vidchopper-chapter-config-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const Path json_path = root / "chapters.json";
    write_text(json_path,
        R"json({
  "$schema": "../docs/schemas/chapter-config.schema.json",
  "version": 1,
  "output": {
    "folder": "%source%_clips",
    "namingPattern": "%index%_%name%"
  },
  "encoder": {
    "crf": 19,
    "cq": 24,
    "preset": "fast",
    "threads": 3
  },
  "chapters": [
    {
      "name": "Opening 🎬",
      "start": 0,
      "end": "00:01:00.000"
    },
    {
      "name": "Middle",
      "start": "60000",
      "end": 150000,
      "outputName": "Middle Clean"
    }
  ]
})json");

    const ChapterConfigLoadResult json_result = load_chapter_config(json_path, 180000, ExportSettings {});
    require_success(json_result, "JSON config should load");
    test_support::expect_eq(json_result.config.schema_version, u32 {1}, "JSON schema version");
    test_support::expect_eq(json_result.config.chapters.size(), size_t {2}, "JSON chapter count");
    test_support::expect_eq(json_result.config.chapters[0].name, std::string {"Opening 🎬"}, "Unicode chapter name");
    test_support::expect_eq(json_result.config.chapters[1].start_ms, u64 {60000}, "numeric string time");
    test_support::expect_eq(
        json_result.config.chapters[1].output_name, std::string {"Middle Clean"}, "per-chapter output name");
    test_support::expect_eq(
        json_result.config.settings.output_folder_pattern, std::string {"%source%_clips"}, "output folder override");
    test_support::expect_eq(
        json_result.config.settings.naming_pattern, std::string {"%index%_%name%"}, "output naming override");
    test_support::expect_eq(json_result.config.settings.x264_crf, u8 {19}, "CRF override");
    test_support::expect_eq(json_result.config.settings.nvenc_cq, u8 {24}, "CQ override");
    test_support::expect_eq(json_result.config.settings.x264_preset, std::string {"fast"}, "x264 preset override");
    test_support::expect_eq(json_result.config.settings.nvenc_preset, std::string {"fast"}, "NVENC preset override");
    test_support::expect_eq(json_result.config.settings.ffmpeg_threads, u8 {3}, "thread override");

    const Path yaml_path = root / "chapters.yaml";
    write_text(yaml_path,
        R"yaml($schema: ../docs/schemas/chapter-config.schema.json
version: 1
output:
  folder: "%source%_clips"
  namingPattern: "%index%_%name%"
encoder:
  crf: 19
  cq: 24
  preset: fast
  threads: 3
chapters:
  - name: "Opening 🎬"
    start: "00:00:00.000"
    end: 60000
  - name: Middle
    start: "00:01:00.000"
    end: 150000
    outputName: "Middle Clean"
)yaml");

    const ChapterConfigLoadResult yaml_result = load_chapter_config(yaml_path, 180000, ExportSettings {});
    require_success(yaml_result, "YAML config should load");
    test_support::expect_eq(yaml_result.config.chapters, json_result.config.chapters, "JSON/YAML chapters match");
    test_support::expect_eq(yaml_result.config.settings, json_result.config.settings, "JSON/YAML settings match");

    const Path yml_path = root / "minimal.yml";
    write_text(yml_path,
        R"yaml(version: 1
chapters:
  - name: Short
    start: "00:00:00"
    end: "00:01:00"
)yaml");
    const ChapterConfigLoadResult yml_result = load_chapter_config(yml_path, 60000, ExportSettings {});
    require_success(yml_result, "YML extension should load");
    test_support::expect_eq(yml_result.config.chapters.front().end_ms, u64 {60000}, "YML timestamp");

    const Path unknown_extension_path = root / "chapters.txt";
    write_text(unknown_extension_path, "chapters: []\n");
    require_failure(load_chapter_config(unknown_extension_path, 60000, ExportSettings {}),
        "unknown extension should fail",
        "unknown chapter config extension");

    const Path missing_path = root / "missing.json";
    require_failure(
        load_chapter_config(missing_path, 60000, ExportSettings {}), "missing file should fail", "could not read");

    const Path malformed_json_path = root / "malformed.json";
    write_text(malformed_json_path, R"json({"version": 1, "chapters": [)json");
    const ChapterConfigLoadResult malformed_json = load_chapter_config(malformed_json_path, 60000, ExportSettings {});
    require_failure(malformed_json, "malformed JSON should fail", "malformed.json");
    test_support::expect_true(contains(malformed_json.error_message, "byte"), "JSON error should include location");

    const Path malformed_yaml_path = root / "malformed.yaml";
    write_text(malformed_yaml_path, "version: 1\nchapters: [\n");
    const ChapterConfigLoadResult malformed_yaml = load_chapter_config(malformed_yaml_path, 60000, ExportSettings {});
    require_failure(malformed_yaml, "malformed YAML should fail", "malformed.yaml");
    test_support::expect_true(contains(malformed_yaml.error_message, "line"), "YAML error should include line");

    const Path unknown_root_path = root / "unknown-root.json";
    write_text(unknown_root_path,
        R"json({"chapters": [{"name": "Opening", "start": 0, "end": 60000}], "mispelled": true})json");
    require_failure(load_chapter_config(unknown_root_path, 60000, ExportSettings {}),
        "unknown root field should fail",
        "unknown top-level field");

    const Path unknown_nested_path = root / "unknown-nested.yaml";
    write_text(unknown_nested_path,
        R"yaml(version: 1
chapters:
  - name: Opening
    start: "00:00:00"
    end: "00:01:00"
    finish: "00:01:00"
)yaml");
    require_failure(load_chapter_config(unknown_nested_path, 60000, ExportSettings {}),
        "unknown chapter field should fail",
        "unknown chapter field");

    const Path invalid_yaml_type_path = root / "invalid-yaml-type.yaml";
    write_text(invalid_yaml_type_path,
        R"yaml(version: 1
encoder:
  crf: "19"
chapters:
  - name: 123
    start: "00:00:00"
    end: "00:01:00"
)yaml");
    require_failure(load_chapter_config(invalid_yaml_type_path, 60000, ExportSettings {}),
        "quoted YAML integer and numeric name should fail",
        "encoder.crf");

    const Path invalid_yaml_name_path = root / "invalid-yaml-name.yaml";
    write_text(invalid_yaml_name_path,
        R"yaml(version: 1
encoder:
  crf: 19
chapters:
  - name: 123
    start: "00:00:00"
    end: "00:01:00"
)yaml");
    require_failure(load_chapter_config(invalid_yaml_name_path, 60000, ExportSettings {}),
        "numeric YAML chapter name should fail",
        "name must be a string");

    const Path invalid_yaml_version_path = root / "invalid-yaml-version.yaml";
    write_text(invalid_yaml_version_path,
        R"yaml(version: "1"
chapters:
  - name: Opening
    start: "00:00:00"
    end: "00:01:00"
)yaml");
    require_failure(load_chapter_config(invalid_yaml_version_path, 60000, ExportSettings {}),
        "quoted YAML version should fail",
        "version must be 1");

    const Path invalid_version_path = root / "invalid-version.json";
    write_text(
        invalid_version_path, R"json({"version": 2, "chapters": [{"name": "Opening", "start": 0, "end": 60000}]})json");
    require_failure(load_chapter_config(invalid_version_path, 60000, ExportSettings {}),
        "unsupported version should fail",
        "version must be 1");

    const Path invalid_type_path = root / "invalid-type.json";
    write_text(invalid_type_path,
        R"json({"version": 1, "encoder": {"crf": "fast"}, "chapters": [{"name": "Opening", "start": 0, "end": 60000}]})json");
    require_failure(load_chapter_config(invalid_type_path, 60000, ExportSettings {}),
        "invalid encoder type should fail",
        "encoder.crf");

    const Path missing_chapters_path = root / "missing-chapters.json";
    write_text(missing_chapters_path, R"json({"version": 1})json");
    require_failure(load_chapter_config(missing_chapters_path, 60000, ExportSettings {}),
        "missing chapters should fail",
        "chapters");

    const Path invalid_time_path = root / "invalid-time.json";
    write_text(invalid_time_path,
        R"json({"version": 1, "chapters": [{"name": "Opening", "start": "not-a-time", "end": "00:01:00"}]})json");
    require_failure(load_chapter_config(invalid_time_path, 60000, ExportSettings {}),
        "invalid timestamp should fail",
        "invalid timestamp");

    const Path invalid_order_path = root / "invalid-order.yaml";
    write_text(invalid_order_path,
        R"yaml(version: 1
chapters:
  - name: Backwards
    start: "00:02:00"
    end: "00:01:00"
)yaml");
    require_failure(load_chapter_config(invalid_order_path, 180000, ExportSettings {}),
        "invalid order should fail",
        "Chapter end time");

    const Path duration_path = root / "duration.json";
    write_text(
        duration_path, R"json({"version": 1, "chapters": [{"name": "Too long", "start": 0, "end": 60001}]})json");
    require_failure(load_chapter_config(duration_path, 60000, ExportSettings {}),
        "source-duration violation should fail",
        "source duration");

    std::filesystem::remove_all(root);
    return 0;
}
