#include "cli/chapter_config.h"
#include "test_support.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>

using namespace vidchopper;

namespace {

auto write_text(const Path& path, const std::string& text) -> void {
    std::filesystem::create_directories(path.parent_path());
    auto stream = std::ofstream {path};
    stream << text;
}

[[nodiscard]] auto contains(const std::string& text, const std::string& needle) -> bool {
    return text.find(needle) != std::string::npos;
}

} // namespace

auto main() -> int {
    const auto root = Path {std::filesystem::temp_directory_path() / "vidchopper-chapter-config"};
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto json_path = Path {root / "chapters.json"};
    write_text(json_path,
        std::string {R"json(
{
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
      "name": "Opening",
      "start": "00:00:00.000",
      "end": "00:01:00.000"
    },
    {
      "name": "Middle",
      "start": "00:01:00.000",
      "end": "00:02:30.000"
    }
  ]
}
)json"});

    const ChapterConfigLoadResult json_result = load_chapter_config(json_path, 180000, ExportSettings {});
    const std::size_t json_chapter_count = json_result.config.chapters.size();
    const std::string json_first_name = json_result.config.chapters[0].name;
    const u64 json_second_start = json_result.config.chapters[1].start_ms;
    const std::string json_output_folder = json_result.config.settings.output_folder_pattern;
    const std::string json_naming_pattern = json_result.config.settings.naming_pattern;
    test_support::expect_true(json_result.ok(), "JSON config should load");
    test_support::expect_eq(json_chapter_count, std::size_t {2}, "JSON should load two chapters");
    test_support::expect_eq(json_first_name, std::string {"Opening"}, "JSON chapter name");
    test_support::expect_eq(json_second_start, 60000ULL, "JSON chapter start");
    test_support::expect_eq(json_output_folder, std::string {"%source%_clips"}, "folder override");
    test_support::expect_eq(json_naming_pattern, std::string {"%index%_%name%"}, "naming override");
    test_support::expect_eq(json_result.config.settings.x264_crf, u8 {19}, "CRF override");
    test_support::expect_eq(json_result.config.settings.nvenc_cq, u8 {24}, "CQ override");
    test_support::expect_eq(json_result.config.settings.x264_preset, std::string {"fast"}, "x264 preset");
    test_support::expect_eq(json_result.config.settings.nvenc_preset, std::string {"fast"}, "NVENC preset");
    test_support::expect_eq(json_result.config.settings.ffmpeg_threads, u8 {3}, "thread override");

    const auto yaml_path = Path {root / "chapters.yaml"};
    write_text(yaml_path,
        std::string {R"yaml(
$schema: ../docs/schemas/chapter-config.schema.json
version: 1
output:
  folder: "%source%_clips"
  namingPattern: "%index%_%name%"
encoder:
  crf: 21
  cq: 25
  preset: medium
  threads: 2
chapters:
  - name: Opening
    start: "00:00:00.000"
    end: "00:01:00.000"
  - name: Middle
    start: "00:01:00.000"
    end: "00:02:30.000"
)yaml"});

    const ChapterConfigLoadResult yaml_result = load_chapter_config(yaml_path, 180000, ExportSettings {});
    const std::size_t yaml_chapter_count = yaml_result.config.chapters.size();
    test_support::expect_true(yaml_result.ok(), "YAML config should load");
    test_support::expect_eq(yaml_chapter_count, std::size_t {2}, "YAML should load two chapters");
    test_support::expect_eq(yaml_result.config.settings.x264_crf, u8 {21}, "YAML CRF override");
    test_support::expect_eq(yaml_result.config.settings.ffmpeg_threads, u8 {2}, "YAML thread override");

    const auto yml_path = Path {root / "chapters.yml"};
    write_text(yml_path,
        std::string {R"yaml(
version: 1
chapters:
  - name: Short
    start: "00:00:00.000"
    end: "00:01:00.000"
)yaml"});

    const ChapterConfigLoadResult yml_result = load_chapter_config(yml_path, 90000, ExportSettings {});
    const std::string yml_chapter_name = yml_result.config.chapters.front().name;
    test_support::expect_true(yml_result.ok(), "YML extension should load");
    test_support::expect_eq(yml_chapter_name, std::string {"Short"}, "YML chapter name");

    const auto bad_extension_path = Path {root / "chapters.txt"};
    write_text(bad_extension_path, "chapters: []\n");
    const ChapterConfigLoadResult bad_extension = load_chapter_config(bad_extension_path, 90000, ExportSettings {});
    const bool extension_error = contains(bad_extension.error_message, "unknown chapter config extension");
    test_support::expect_true(!bad_extension.ok(), "unknown extension should fail");
    test_support::expect_true(extension_error, "extension error");

    const auto unknown_key_path = Path {root / "unknown-key.yaml"};
    write_text(unknown_key_path,
        std::string {R"yaml(
version: 1
chapters:
  - name: Opening
    start: "00:00:00.000"
    finish: "00:01:00.000"
)yaml"});
    const ChapterConfigLoadResult unknown_key = load_chapter_config(unknown_key_path, 90000, ExportSettings {});
    const bool unknown_key_error = contains(unknown_key.error_message, "unknown chapter key");
    const bool unknown_key_path_error = contains(unknown_key.error_message, "unknown-key.yaml");
    test_support::expect_true(!unknown_key.ok(), "unknown chapter key should fail");
    test_support::expect_true(unknown_key_error, "unknown key error");
    test_support::expect_true(unknown_key_path_error, "error should include path");

    const auto invalid_time_path = Path {root / "invalid-time.json"};
    write_text(invalid_time_path,
        std::string {R"json(
{
  "version": 1,
  "chapters": [
    {
      "name": "Opening",
      "start": "not-a-time",
      "end": "00:01:00.000"
    }
  ]
}
)json"});
    const ChapterConfigLoadResult invalid_time = load_chapter_config(invalid_time_path, 90000, ExportSettings {});
    const bool timestamp_error = contains(invalid_time.error_message, "invalid timestamp");
    test_support::expect_true(!invalid_time.ok(), "invalid timestamp should fail");
    test_support::expect_true(timestamp_error, "timestamp error");

    const auto invalid_order_path = Path {root / "invalid-order.yaml"};
    write_text(invalid_order_path,
        std::string {R"yaml(
version: 1
chapters:
  - name: Backwards
    start: "00:02:00.000"
    end: "00:01:00.000"
)yaml"});
    const ChapterConfigLoadResult invalid_order = load_chapter_config(invalid_order_path, 180000, ExportSettings {});
    const bool core_validation_error = contains(invalid_order.error_message, "Chapter end time");
    test_support::expect_true(!invalid_order.ok(), "invalid time order should fail validation");
    test_support::expect_true(core_validation_error, "core validation error");

    std::filesystem::remove_all(root);
    return 0;
}
