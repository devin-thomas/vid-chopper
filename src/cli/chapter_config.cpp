#include "cli/chapter_config.h"

#include "core/chapter_plan.h"
#include "core/string_utils.h"
#include "core/timecode.h"

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace vidchopper {

namespace {

using Json = nlohmann::json;

struct ParseResult {
    bool success {false};
    ChapterConfig config {};
    std::string error_message {};

    [[nodiscard]] auto ok() const noexcept -> bool {
        return success;
    }
};

constexpr auto root_keys = std::to_array<std::string_view>({"$schema", "version", "output", "encoder", "chapters"});
constexpr auto output_keys = std::to_array<std::string_view>({"folder", "namingPattern"});
constexpr auto encoder_keys = std::to_array<std::string_view>({"crf", "cq", "preset", "threads"});
constexpr auto chapter_keys = std::to_array<std::string_view>({"name", "start", "end", "outputName"});

[[nodiscard]] auto failure(const Path& path, const std::string_view message) -> ParseResult {
    return ParseResult {.error_message = path.string() + ": " + std::string {message}};
}

[[nodiscard]] auto contains_key(const std::string_view key, const std::span<const std::string_view> keys) -> bool {
    return std::ranges::find(keys, key) != keys.end();
}

[[nodiscard]] auto read_text_file(const Path& path) -> std::optional<std::string> {
    auto stream = std::ifstream {path, std::ios::binary};
    if (!stream.is_open()) {
        return std::nullopt;
    }

    auto buffer = std::ostringstream {};
    buffer << stream.rdbuf();
    if (!stream.good() && !stream.eof()) {
        return std::nullopt;
    }

    return buffer.str();
}

[[nodiscard]] auto normalized_extension(const Path& path) -> std::string {
    return to_lower_copy(path.extension().string());
}

[[nodiscard]] auto parse_unsigned_milliseconds(const std::string_view value) -> std::optional<u64> {
    if (value.empty()) {
        return std::nullopt;
    }

    auto parsed = u64 {0};
    const char* const first = value.data();
    const char* const last = value.data() + value.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc {} || result.ptr != last) {
        return std::nullopt;
    }

    return parsed;
}

[[nodiscard]] auto parse_timestamp(const std::string_view raw_value) -> std::optional<u64> {
    const std::string value = trim_copy(raw_value);
    if (value.find(':') == std::string::npos) {
        return parse_unsigned_milliseconds(value);
    }

    return parse_millisecond_timecode(value);
}

[[nodiscard]] auto validate_non_empty_string(
    const std::string& value, const Path& path, const std::string_view field) -> std::optional<ParseResult> {
    if (trim_copy(value).empty()) {
        return failure(path, std::string {field} + " must not be empty.");
    }

    return std::nullopt;
}

[[nodiscard]] auto json_integer(const Json& value, const u64 maximum) -> std::optional<u8> {
    if (!value.is_number_integer() && !value.is_number_unsigned()) {
        return std::nullopt;
    }

    if (value.is_number_integer() && value.template get<i64>() < 0) {
        return std::nullopt;
    }

    const u64 parsed = value.template get<u64>();
    if (parsed > maximum) {
        return std::nullopt;
    }

    return static_cast<u8>(parsed);
}

[[nodiscard]] auto yaml_integer(const YAML::Node& value, const u64 maximum) -> std::optional<u8> {
    if (!value.IsScalar() || (value.Tag() != "?" && value.Tag() != "tag:yaml.org,2002:int")) {
        return std::nullopt;
    }

    const std::string text = value.as<std::string>();
    const std::optional<u64> parsed = parse_unsigned_milliseconds(text);
    if (!parsed.has_value() || *parsed > maximum) {
        return std::nullopt;
    }

    return static_cast<u8>(*parsed);
}

[[nodiscard]] auto yaml_plain_scalar_is_non_string(const std::string_view raw_value) -> bool {
    const std::string value = to_lower_copy(trim_copy(raw_value));
    if (value.empty() || value == "~" || value == "null" || value == "true" || value == "false" || value == "yes"
        || value == "no" || value == "on" || value == "off") {
        return true;
    }

    if (parse_unsigned_milliseconds(value).has_value()) {
        return true;
    }

    auto signed_value = i64 {0};
    const char* const first = value.data();
    const char* const last = value.data() + value.size();
    const std::from_chars_result signed_result = std::from_chars(first, last, signed_value);
    if (signed_result.ec == std::errc {} && signed_result.ptr == last) {
        return true;
    }

    if (value == ".inf" || value == "+.inf" || value == "-.inf" || value == ".nan") {
        return true;
    }

    if (value.find('.') == std::string::npos && value.find('e') == std::string::npos
        && value.find('E') == std::string::npos) {
        return false;
    }

    char* end = nullptr;
    std::strtod(value.c_str(), &end);
    return end == value.c_str() + value.size();
}

[[nodiscard]] auto yaml_string_value(const YAML::Node& value) -> std::optional<std::string> {
    if (!value.IsScalar()) {
        return std::nullopt;
    }

    const std::string& tag = value.Tag();
    if (tag == "tag:yaml.org,2002:str" || tag == "!") {
        return value.as<std::string>();
    }
    if (tag != "?") {
        return std::nullopt;
    }

    const std::string text = value.as<std::string>();
    if (yaml_plain_scalar_is_non_string(text)) {
        return std::nullopt;
    }

    return text;
}

[[nodiscard]] auto json_timestamp(const Json& value) -> std::optional<u64> {
    if (value.is_string()) {
        return parse_timestamp(value.get<std::string>());
    }

    if (value.is_number_unsigned()) {
        return value.get<u64>();
    }

    if (value.is_number_integer()) {
        const i64 parsed = value.get<i64>();
        if (parsed >= 0) {
            return static_cast<u64>(parsed);
        }
    }

    return std::nullopt;
}

[[nodiscard]] auto yaml_timestamp(const YAML::Node& value) -> std::optional<u64> {
    if (!value.IsScalar()) {
        return std::nullopt;
    }

    return parse_timestamp(value.as<std::string>());
}

[[nodiscard]] auto parse_json(
    const std::string& text, const Path& path, const ExportSettings& base_settings) -> ParseResult {
    Json document;
    try {
        document = Json::parse(text);
    } catch (const Json::parse_error& error) {
        return failure(path, std::format("malformed JSON at byte {}: {}", error.byte, error.what()));
    }

    if (!document.is_object()) {
        return failure(path, "top-level value must be an object.");
    }

    auto config = ChapterConfig {.settings = base_settings};
    for (const auto& [key, value] : document.items()) {
        if (!contains_key(key, root_keys)) {
            return failure(path, "unknown top-level field '" + key + "'.");
        }

        if (key == "$schema") {
            if (!value.is_string()) {
                return failure(path, "$schema must be a string.");
            }
        } else if (key == "version") {
            if (!value.is_number_integer() || value.get<i64>() != 1) {
                return failure(path, "version must be 1.");
            }
        } else if (key == "output") {
            if (!value.is_object()) {
                return failure(path, "output must be an object.");
            }

            for (const auto& [output_key, output_value] : value.items()) {
                if (!contains_key(output_key, output_keys)) {
                    return failure(path, "unknown output field '" + output_key + "'.");
                }
                if (!output_value.is_string()) {
                    return failure(path, "output." + output_key + " must be a string.");
                }
                const std::string output_text = output_value.get<std::string>();
                if (const std::optional<ParseResult> invalid =
                        validate_non_empty_string(output_text, path, "output." + output_key);
                    invalid.has_value()) {
                    return *invalid;
                }

                if (output_key == "folder") {
                    config.settings.output_folder_pattern = output_text;
                } else {
                    config.settings.naming_pattern = output_text;
                }
            }
        } else if (key == "encoder") {
            if (!value.is_object()) {
                return failure(path, "encoder must be an object.");
            }

            for (const auto& [encoder_key, encoder_value] : value.items()) {
                if (!contains_key(encoder_key, encoder_keys)) {
                    return failure(path, "unknown encoder field '" + encoder_key + "'.");
                }

                if (encoder_key == "preset") {
                    if (!encoder_value.is_string()) {
                        return failure(path, "encoder.preset must be a string.");
                    }
                    const std::string preset = encoder_value.get<std::string>();
                    if (const std::optional<ParseResult> invalid =
                            validate_non_empty_string(preset, path, "encoder.preset");
                        invalid.has_value()) {
                        return *invalid;
                    }
                    config.settings.x264_preset = preset;
                    config.settings.nvenc_preset = preset;
                    continue;
                }

                const u64 maximum = encoder_key == "threads" ? 255 : 51;
                const std::optional<u8> parsed = json_integer(encoder_value, maximum);
                if (!parsed.has_value()) {
                    return failure(path, "encoder." + encoder_key + " must be an integer in the supported range.");
                }
                if (encoder_key == "crf") {
                    config.settings.x264_crf = *parsed;
                } else if (encoder_key == "cq") {
                    config.settings.nvenc_cq = *parsed;
                } else {
                    config.settings.ffmpeg_threads = *parsed;
                }
            }
        } else if (key == "chapters") {
            if (!value.is_array()) {
                return failure(path, "chapters must be an array.");
            }
            if (value.empty()) {
                return failure(path, "chapters must contain at least one chapter.");
            }

            config.chapters.reserve(value.size());
            for (auto index = std::size_t {0}; index < value.size(); ++index) {
                const Json& chapter = value[index];
                const std::string prefix = std::format("chapters[{}].", index);
                if (!chapter.is_object()) {
                    return failure(path, prefix + "must be an object.");
                }
                for (const auto& [chapter_key, unused] : chapter.items()) {
                    if (!contains_key(chapter_key, chapter_keys)) {
                        return failure(path, "unknown chapter field '" + chapter_key + "'.");
                    }
                }

                for (const std::string_view required : {"name", "start", "end"}) {
                    if (!chapter.contains(required)) {
                        return failure(path, prefix + std::string {required} + " is required.");
                    }
                }
                if (!chapter["name"].is_string() || trim_copy(chapter["name"].get<std::string>()).empty()) {
                    return failure(path, prefix + "name is required and must be a string.");
                }
                const std::optional<u64> start_ms = json_timestamp(chapter["start"]);
                const std::optional<u64> end_ms = json_timestamp(chapter["end"]);
                if (!start_ms.has_value()) {
                    return failure(path, prefix + "start has an invalid timestamp.");
                }
                if (!end_ms.has_value()) {
                    return failure(path, prefix + "end has an invalid timestamp.");
                }

                auto output_name = std::string {};
                if (chapter.contains("outputName")) {
                    if (!chapter["outputName"].is_string()) {
                        return failure(path, prefix + "outputName must be a string.");
                    }
                    output_name = chapter["outputName"].get<std::string>();
                    if (output_name.empty()) {
                        return failure(path, prefix + "outputName must not be empty.");
                    }
                }

                config.chapters.push_back(ChapterSegment {
                    .name = chapter["name"].get<std::string>(),
                    .start_ms = *start_ms,
                    .end_ms = *end_ms,
                    .output_name = std::move(output_name),
                });
            }
        }
    }

    if (!document.contains("chapters")) {
        return failure(path, "missing required top-level field 'chapters'.");
    }

    return ParseResult {.success = true, .config = std::move(config)};
}

[[nodiscard]] auto yaml_key(const YAML::Node& key_node) -> std::optional<std::string> {
    if (!key_node.IsScalar()) {
        return std::nullopt;
    }
    return key_node.as<std::string>();
}

[[nodiscard]] auto parse_yaml(
    const std::string& text, const Path& path, const ExportSettings& base_settings) -> ParseResult {
    YAML::Node document;
    try {
        document = YAML::Load(text);
    } catch (const YAML::ParserException& error) {
        return failure(path,
            std::format(
                "malformed YAML at line {}, column {}: {}", error.mark.line + 1, error.mark.column + 1, error.what()));
    }

    if (!document.IsMap()) {
        return failure(path, "top-level value must be a mapping.");
    }

    auto config = ChapterConfig {.settings = base_settings};
    for (const auto& entry : document) {
        const std::optional<std::string> key = yaml_key(entry.first);
        if (!key.has_value()) {
            return failure(path, "top-level field names must be strings.");
        }
        if (!contains_key(*key, root_keys)) {
            return failure(path, "unknown top-level field '" + *key + "'.");
        }

        const YAML::Node value = entry.second;
        if (*key == "$schema") {
            if (!yaml_string_value(value).has_value()) {
                return failure(path, "$schema must be a string.");
            }
        } else if (*key == "version") {
            const std::optional<u8> version = yaml_integer(value, 1);
            if (!version.has_value() || *version != 1) {
                return failure(path, "version must be 1.");
            }
        } else if (*key == "output") {
            if (!value.IsMap()) {
                return failure(path, "output must be a mapping.");
            }
            for (const auto& output_entry : value) {
                const std::optional<std::string> output_key = yaml_key(output_entry.first);
                if (!output_key.has_value() || !contains_key(*output_key, output_keys)) {
                    return failure(path,
                        output_key.has_value() ? "unknown output field '" + *output_key + "'."
                                               : "output field names must be strings.");
                }
                const std::optional<std::string> output_text = yaml_string_value(output_entry.second);
                if (!output_text.has_value()) {
                    return failure(path, "output." + *output_key + " must be a string.");
                }
                if (const std::optional<ParseResult> invalid =
                        validate_non_empty_string(*output_text, path, "output." + *output_key);
                    invalid.has_value()) {
                    return *invalid;
                }
                if (*output_key == "folder") {
                    config.settings.output_folder_pattern = *output_text;
                } else {
                    config.settings.naming_pattern = *output_text;
                }
            }
        } else if (*key == "encoder") {
            if (!value.IsMap()) {
                return failure(path, "encoder must be a mapping.");
            }
            for (const auto& encoder_entry : value) {
                const std::optional<std::string> encoder_key = yaml_key(encoder_entry.first);
                if (!encoder_key.has_value() || !contains_key(*encoder_key, encoder_keys)) {
                    return failure(path,
                        encoder_key.has_value() ? "unknown encoder field '" + *encoder_key + "'."
                                                : "encoder field names must be strings.");
                }
                if (*encoder_key == "preset") {
                    const std::optional<std::string> preset = yaml_string_value(encoder_entry.second);
                    if (!preset.has_value()) {
                        return failure(path, "encoder.preset must be a string.");
                    }
                    if (const std::optional<ParseResult> invalid =
                            validate_non_empty_string(*preset, path, "encoder.preset");
                        invalid.has_value()) {
                        return *invalid;
                    }
                    config.settings.x264_preset = *preset;
                    config.settings.nvenc_preset = *preset;
                    continue;
                }
                const u64 maximum = *encoder_key == "threads" ? 255 : 51;
                const std::optional<u8> parsed = yaml_integer(encoder_entry.second, maximum);
                if (!parsed.has_value()) {
                    return failure(path, "encoder." + *encoder_key + " must be an integer in the supported range.");
                }
                if (*encoder_key == "crf") {
                    config.settings.x264_crf = *parsed;
                } else if (*encoder_key == "cq") {
                    config.settings.nvenc_cq = *parsed;
                } else {
                    config.settings.ffmpeg_threads = *parsed;
                }
            }
        } else if (*key == "chapters") {
            if (!value.IsSequence()) {
                return failure(path, "chapters must be a sequence.");
            }
            if (value.size() == 0) {
                return failure(path, "chapters must contain at least one chapter.");
            }
            config.chapters.reserve(value.size());
            for (auto index = std::size_t {0}; index < value.size(); ++index) {
                const YAML::Node chapter = value[index];
                const std::string prefix = std::format("chapters[{}].", index);
                if (!chapter.IsMap()) {
                    return failure(path, prefix + "must be a mapping.");
                }
                for (const auto& chapter_entry : chapter) {
                    const std::optional<std::string> chapter_key = yaml_key(chapter_entry.first);
                    if (!chapter_key.has_value() || !contains_key(*chapter_key, chapter_keys)) {
                        return failure(path,
                            chapter_key.has_value() ? "unknown chapter field '" + *chapter_key + "'."
                                                    : "chapter field names must be strings.");
                    }
                }
                for (const std::string_view required : {"name", "start", "end"}) {
                    if (!chapter[std::string {required}]) {
                        return failure(path, prefix + std::string {required} + " is required.");
                    }
                }
                const std::optional<std::string> name_value = yaml_string_value(chapter["name"]);
                if (!name_value.has_value()) {
                    return failure(path, prefix + "name must be a string.");
                }
                const std::string& name = *name_value;
                if (trim_copy(name).empty()) {
                    return failure(path, prefix + "name is required.");
                }
                const std::optional<u64> start_ms = yaml_timestamp(chapter["start"]);
                const std::optional<u64> end_ms = yaml_timestamp(chapter["end"]);
                if (!start_ms.has_value()) {
                    return failure(path, prefix + "start has an invalid timestamp.");
                }
                if (!end_ms.has_value()) {
                    return failure(path, prefix + "end has an invalid timestamp.");
                }

                auto output_name = std::string {};
                if (chapter["outputName"]) {
                    const std::optional<std::string> output_name_value = yaml_string_value(chapter["outputName"]);
                    if (!output_name_value.has_value()) {
                        return failure(path, prefix + "outputName must be a string.");
                    }
                    output_name = *output_name_value;
                    if (output_name.empty()) {
                        return failure(path, prefix + "outputName must not be empty.");
                    }
                }

                config.chapters.push_back(ChapterSegment {
                    .name = name,
                    .start_ms = *start_ms,
                    .end_ms = *end_ms,
                    .output_name = std::move(output_name),
                });
            }
        }
    }

    if (!document["chapters"]) {
        return failure(path, "missing required top-level field 'chapters'.");
    }

    return ParseResult {.success = true, .config = std::move(config)};
}

[[nodiscard]] auto validate_config(
    ParseResult parsed, const Path& path, const u64 source_duration_ms) -> ChapterConfigLoadResult {
    if (!parsed.ok()) {
        return ChapterConfigLoadResult {.error_message = std::move(parsed.error_message)};
    }

    const ValidationResult validation =
        validate_chapters(parsed.config.chapters, source_duration_ms, parsed.config.settings);
    if (!validation.ok()) {
        const ValidationIssue& issue = validation.issues.front();
        return ChapterConfigLoadResult {
            .error_message = std::format("{}: chapters[{}]: {}", path.string(), issue.chapter_index, issue.message),
        };
    }

    return ChapterConfigLoadResult {.success = true, .config = std::move(parsed.config)};
}

} // namespace

auto ChapterConfigLoadResult::ok() const noexcept -> bool {
    return success;
}

auto load_chapter_config(const Path& config_path,
    const u64 source_duration_ms,
    const ExportSettings& base_settings) -> ChapterConfigLoadResult {
    const std::string extension = normalized_extension(config_path);
    if (extension != ".json" && extension != ".yaml" && extension != ".yml") {
        return ChapterConfigLoadResult {.error_message = config_path.string() + ": unknown chapter config extension."};
    }

    const std::optional<std::string> text = read_text_file(config_path);
    if (!text.has_value()) {
        return ChapterConfigLoadResult {.error_message = config_path.string() + ": could not read chapter config."};
    }

    if (extension == ".json") {
        return validate_config(parse_json(*text, config_path, base_settings), config_path, source_duration_ms);
    }

    return validate_config(parse_yaml(*text, config_path, base_settings), config_path, source_duration_ms);
}

} // namespace vidchopper
