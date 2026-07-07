#include "cli/chapter_config.h"

#include "core/chapter_plan.h"
#include "core/string_utils.h"
#include "core/timecode.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace vidchopper {

namespace {

using KeyValue = std::pair<std::string, std::string>;

struct ChapterDraft {
    std::string name;
    std::string start;
    std::string end;
    u64 line_number {0};
};

struct ConfigDraft {
    ExportSettings settings;
    std::vector<ChapterDraft> chapters;
};

struct DraftResult {
    bool success {false};
    ConfigDraft draft;
    std::string error_message;

    [[nodiscard]] auto ok() const noexcept -> bool {
        return success;
    }
};

enum class ConfigSection : u8 {
    Root = 0,
    Output = 1,
    Encoder = 2,
    Chapters = 3,
};

constexpr auto root_keys = std::to_array<std::string_view>({
    "$schema",
    "version",
    "output",
    "encoder",
    "chapters",
});
constexpr auto output_keys = std::to_array<std::string_view>({
    "folder",
    "namingPattern",
});
constexpr auto encoder_keys = std::to_array<std::string_view>({
    "crf",
    "cq",
    "preset",
    "threads",
});
constexpr auto chapter_keys = std::to_array<std::string_view>({
    "name",
    "start",
    "end",
    "outputName",
});

[[nodiscard]] auto failure(const Path& path, const std::string& message) -> DraftResult {
    return DraftResult {.error_message = path.string() + ": " + message};
}

[[nodiscard]] auto failure(const Path& path, const u64 line_number, const std::string& message) -> DraftResult {
    const std::string line_prefix = "line " + std::to_string(line_number) + ": ";
    return failure(path, line_prefix + message);
}

[[nodiscard]] auto read_text_file(const Path& path) -> std::optional<std::string> {
    auto stream = std::ifstream {path};
    if (!stream.good()) {
        return std::nullopt;
    }

    auto buffer = std::ostringstream {};
    buffer << stream.rdbuf();
    return buffer.str();
}

[[nodiscard]] auto contains_key(const std::string_view key, const auto& keys) -> bool {
    return std::ranges::find(keys, key) != keys.end();
}

[[nodiscard]] auto normalized_extension(const Path& path) -> std::string {
    return to_lower_copy(path.extension().string());
}

[[nodiscard]] auto unquote(std::string value) -> std::string {
    value = trim_copy(value);
    if (value.size() < 2) {
        return value;
    }

    const bool has_double_quotes = value.front() == '"' && value.back() == '"';
    const bool has_single_quotes = value.front() == '\'' && value.back() == '\'';
    if (has_double_quotes || has_single_quotes) {
        value = value.substr(1, value.size() - 2);
    }

    return value;
}

[[nodiscard]] auto strip_json_tail(std::string value) -> std::string {
    value = trim_copy(value);
    while (!value.empty() && (value.back() == ',' || value.back() == '}' || value.back() == ']')) {
        value.pop_back();
        value = trim_copy(value);
    }

    return value;
}

[[nodiscard]] auto parse_key_value(const std::string& line) -> std::optional<KeyValue> {
    const std::size_t separator = line.find(':');
    if (separator == std::string::npos) {
        return std::nullopt;
    }

    std::string key = trim_copy(std::string_view {line}.substr(0, separator));
    std::string value = trim_copy(std::string_view {line}.substr(separator + 1));
    key = unquote(key);
    value = unquote(strip_json_tail(value));

    return KeyValue {key, value};
}

[[nodiscard]] auto parse_u8_value(const std::string_view text, const u8 maximum) -> std::optional<u8> {
    const std::string trimmed = trim_copy(text);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    auto parsed = u32 {0};
    const char* const first = trimmed.data();
    const char* const last = trimmed.data() + trimmed.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc {} || result.ptr != last || parsed > maximum) {
        return std::nullopt;
    }

    return static_cast<u8>(parsed);
}

auto apply_output_value(ExportSettings& settings, const std::string_view key, const std::string_view value) -> void {
    if (key == "folder") {
        settings.output_folder_pattern = std::string {value};
        return;
    }

    if (key == "namingPattern") {
        settings.naming_pattern = std::string {value};
    }
}

[[nodiscard]] auto apply_encoder_value(ExportSettings& settings,
    const std::string_view key,
    const std::string_view value,
    const Path& path,
    const u64 line_number) -> DraftResult {
    if (key == "crf") {
        const std::optional<u8> parsed = parse_u8_value(value, u8 {51});
        if (!parsed.has_value()) {
            return failure(path, line_number, "encoder.crf must be an integer from 0 to 51.");
        }
        settings.x264_crf = *parsed;
    } else if (key == "cq") {
        const std::optional<u8> parsed = parse_u8_value(value, u8 {51});
        if (!parsed.has_value()) {
            return failure(path, line_number, "encoder.cq must be an integer from 0 to 51.");
        }
        settings.nvenc_cq = *parsed;
    } else if (key == "threads") {
        const std::optional<u8> parsed = parse_u8_value(value, u8 {255});
        if (!parsed.has_value()) {
            return failure(path, line_number, "encoder.threads must be an integer from 0 to 255.");
        }
        settings.ffmpeg_threads = *parsed;
    } else if (key == "preset") {
        settings.x264_preset = std::string {value};
        settings.nvenc_preset = std::string {value};
    }

    return DraftResult {.success = true};
}

auto apply_chapter_value(ChapterDraft& chapter, const std::string_view key, const std::string_view value) -> void {
    if (key == "name") {
        chapter.name = std::string {value};
        return;
    }

    if (key == "start") {
        chapter.start = std::string {value};
        return;
    }

    if (key == "end") {
        chapter.end = std::string {value};
    }
}

[[nodiscard]] auto parse_json_key(const std::string& line) -> std::optional<KeyValue> {
    const std::size_t key_start = line.find('"');
    if (key_start == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t key_end = line.find('"', key_start + 1);
    if (key_end == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t separator = line.find(':', key_end + 1);
    if (separator == std::string::npos) {
        return std::nullopt;
    }

    const std::string key = line.substr(key_start + 1, key_end - key_start - 1);
    std::string value = trim_copy(std::string_view {line}.substr(separator + 1));
    value = unquote(strip_json_tail(value));
    return KeyValue {key, value};
}

[[nodiscard]] auto parse_json_config(
    const std::string& text, const Path& path, const ExportSettings& base_settings) -> DraftResult {
    auto draft = ConfigDraft {.settings = base_settings};
    auto section = ConfigSection::Root;
    auto current_chapter = ChapterDraft {};
    bool has_open_chapter {false};
    auto line_number = u64 {0};

    auto stream = std::istringstream {text};
    auto line = std::string {};
    while (std::getline(stream, line)) {
        ++line_number;
        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed == "[" || trimmed == ",") {
            continue;
        }

        if (section == ConfigSection::Chapters && trimmed.starts_with('{')) {
            current_chapter = ChapterDraft {.line_number = line_number};
            has_open_chapter = true;
            continue;
        }

        if (trimmed == "{") {
            continue;
        }

        if (section == ConfigSection::Chapters && trimmed.starts_with('}')) {
            if (has_open_chapter) {
                draft.chapters.push_back(current_chapter);
                has_open_chapter = false;
            }
            continue;
        }

        if (trimmed.starts_with(']')) {
            section = ConfigSection::Root;
            continue;
        }

        if (trimmed.starts_with('}')) {
            section = ConfigSection::Root;
            continue;
        }

        const std::optional<KeyValue> parsed = parse_json_key(trimmed);
        if (!parsed.has_value()) {
            return failure(path, line_number, "malformed JSON config line.");
        }

        const std::string& key = parsed->first;
        const std::string& value = parsed->second;
        if (section == ConfigSection::Root) {
            if (!contains_key(key, root_keys)) {
                return failure(path, line_number, "unknown top-level key '" + key + "'.");
            }

            if (key == "output") {
                section = ConfigSection::Output;
            } else if (key == "encoder") {
                section = ConfigSection::Encoder;
            } else if (key == "chapters") {
                section = ConfigSection::Chapters;
            } else if (key == "version" && value != "1") {
                return failure(path, line_number, "version must be 1.");
            }
            continue;
        }

        if (section == ConfigSection::Output) {
            if (!contains_key(key, output_keys)) {
                return failure(path, line_number, "unknown output key '" + key + "'.");
            }
            apply_output_value(draft.settings, key, value);
        } else if (section == ConfigSection::Encoder) {
            if (!contains_key(key, encoder_keys)) {
                return failure(path, line_number, "unknown encoder key '" + key + "'.");
            }
            const DraftResult result = apply_encoder_value(draft.settings, key, value, path, line_number);
            if (!result.ok()) {
                return result;
            }
        } else if (section == ConfigSection::Chapters) {
            if (!has_open_chapter) {
                return failure(path, line_number, "chapter field appears outside a chapter object.");
            }

            if (!contains_key(key, chapter_keys)) {
                return failure(path, line_number, "unknown chapter key '" + key + "'.");
            }
            apply_chapter_value(current_chapter, key, value);
        }
    }

    return DraftResult {.success = true, .draft = std::move(draft)};
}

[[nodiscard]] auto leading_spaces(const std::string& line) -> std::size_t {
    auto count = std::size_t {0};
    while (count < line.size() && line[count] == ' ') {
        ++count;
    }

    return count;
}

[[nodiscard]] auto parse_yaml_config(
    const std::string& text, const Path& path, const ExportSettings& base_settings) -> DraftResult {
    auto draft = ConfigDraft {.settings = base_settings};
    auto section = ConfigSection::Root;
    auto current_chapter = ChapterDraft {};
    bool has_open_chapter {false};
    auto line_number = u64 {0};

    auto stream = std::istringstream {text};
    auto line = std::string {};
    while (std::getline(stream, line)) {
        ++line_number;
        const std::string trimmed = trim_copy(line);
        if (trimmed.empty() || trimmed.starts_with('#')) {
            continue;
        }

        const std::size_t indent = leading_spaces(line);
        if (section == ConfigSection::Chapters && trimmed.starts_with("- ")) {
            if (has_open_chapter) {
                draft.chapters.push_back(current_chapter);
            }
            current_chapter = ChapterDraft {.line_number = line_number};
            has_open_chapter = true;
            const std::optional<KeyValue> parsed = parse_key_value(trimmed.substr(2));
            if (parsed.has_value()) {
                if (!contains_key(parsed->first, chapter_keys)) {
                    return failure(path, line_number, "unknown chapter key '" + parsed->first + "'.");
                }
                apply_chapter_value(current_chapter, parsed->first, parsed->second);
            }
            continue;
        }

        if (indent == 0) {
            if (section == ConfigSection::Chapters && has_open_chapter) {
                draft.chapters.push_back(current_chapter);
                has_open_chapter = false;
            }

            const std::optional<KeyValue> parsed = parse_key_value(trimmed);
            if (!parsed.has_value()) {
                return failure(path, line_number, "malformed YAML config line.");
            }

            const std::string& key = parsed->first;
            const std::string& value = parsed->second;
            if (!contains_key(key, root_keys)) {
                return failure(path, line_number, "unknown top-level key '" + key + "'.");
            }

            if (key == "output") {
                section = ConfigSection::Output;
            } else if (key == "encoder") {
                section = ConfigSection::Encoder;
            } else if (key == "chapters") {
                section = ConfigSection::Chapters;
            } else if (key == "version" && value != "1") {
                return failure(path, line_number, "version must be 1.");
            } else {
                section = ConfigSection::Root;
            }
            continue;
        }

        const std::optional<KeyValue> parsed = parse_key_value(trimmed);
        if (!parsed.has_value()) {
            return failure(path, line_number, "malformed YAML config line.");
        }

        const std::string& key = parsed->first;
        const std::string& value = parsed->second;
        if (section == ConfigSection::Output) {
            if (!contains_key(key, output_keys)) {
                return failure(path, line_number, "unknown output key '" + key + "'.");
            }
            apply_output_value(draft.settings, key, value);
        } else if (section == ConfigSection::Encoder) {
            if (!contains_key(key, encoder_keys)) {
                return failure(path, line_number, "unknown encoder key '" + key + "'.");
            }
            const DraftResult result = apply_encoder_value(draft.settings, key, value, path, line_number);
            if (!result.ok()) {
                return result;
            }
        } else if (section == ConfigSection::Chapters) {
            if (!has_open_chapter) {
                return failure(path, line_number, "chapter field appears before the first chapter item.");
            }

            if (!contains_key(key, chapter_keys)) {
                return failure(path, line_number, "unknown chapter key '" + key + "'.");
            }
            apply_chapter_value(current_chapter, key, value);
        }
    }

    if (section == ConfigSection::Chapters && has_open_chapter) {
        draft.chapters.push_back(current_chapter);
    }

    return DraftResult {.success = true, .draft = std::move(draft)};
}

[[nodiscard]] auto convert_chapters(
    const ConfigDraft& draft, const Path& path, const u64 source_duration_ms) -> ChapterConfigLoadResult {
    auto config = ChapterConfig {.settings = draft.settings};
    config.chapters.reserve(draft.chapters.size());

    for (auto index = std::size_t {0}; index < draft.chapters.size(); ++index) {
        const ChapterDraft& row = draft.chapters[index];
        if (trim_copy(row.name).empty()) {
            const std::string message = ": chapters[" + std::to_string(index) + "].name is required.";
            return ChapterConfigLoadResult {.error_message = path.string() + message};
        }

        const std::optional<u64> start_ms = parse_millisecond_timecode(row.start);
        if (!start_ms.has_value()) {
            const std::string message = ": chapters[" + std::to_string(index) + "].start has an invalid timestamp.";
            return ChapterConfigLoadResult {.error_message = path.string() + message};
        }

        const std::optional<u64> end_ms = parse_millisecond_timecode(row.end);
        if (!end_ms.has_value()) {
            const std::string message = ": chapters[" + std::to_string(index) + "].end has an invalid timestamp.";
            return ChapterConfigLoadResult {.error_message = path.string() + message};
        }

        config.chapters.push_back(ChapterSegment {
            .name = row.name,
            .start_ms = *start_ms,
            .end_ms = *end_ms,
        });
    }

    const ValidationResult validation = validate_chapters(config.chapters, source_duration_ms, config.settings);
    if (!validation.ok()) {
        const ValidationIssue& issue = validation.issues.front();
        const std::string message = ": chapters[" + std::to_string(issue.chapter_index) + "]: " + issue.message;
        return ChapterConfigLoadResult {.error_message = path.string() + message};
    }

    return ChapterConfigLoadResult {.success = true, .config = std::move(config)};
}

} // namespace

auto ChapterConfigLoadResult::ok() const noexcept -> bool {
    return success;
}

auto load_chapter_config(const Path& config_path,
    const u64 source_duration_ms,
    const ExportSettings& base_settings) -> ChapterConfigLoadResult {
    const std::string extension = normalized_extension(config_path);
    const bool supported_extension = extension == ".json" || extension == ".yaml" || extension == ".yml";
    if (!supported_extension) {
        return ChapterConfigLoadResult {.error_message = config_path.string() + ": unknown chapter config extension."};
    }

    const std::optional<std::string> text = read_text_file(config_path);
    if (!text.has_value()) {
        return ChapterConfigLoadResult {.error_message = config_path.string() + ": could not read chapter config."};
    }

    const DraftResult draft = extension == ".json" ? parse_json_config(*text, config_path, base_settings)
                                                   : parse_yaml_config(*text, config_path, base_settings);
    if (!draft.ok()) {
        return ChapterConfigLoadResult {.error_message = draft.error_message};
    }

    return convert_chapters(draft.draft, config_path, source_duration_ms);
}

} // namespace vidchopper
