#include "services/probe_service.hpp"

#include "core/path_utils.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <format>
#include <limits>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

namespace vidchopper {

namespace {

using Json = nlohmann::json;

[[nodiscard]] auto bounded_context(std::string context) -> std::string {
    constexpr auto maximum_bytes = size_t {4096};
    if (context.size() > maximum_bytes) {
        context.resize(maximum_bytes);
        context += "... [truncated]";
    }
    return context;
}

[[nodiscard]] auto failure(
    const Path& executable, const Path& source_path, ProcessResult process, const std::string_view detail)
    -> ProbeResult {
    const std::string context =
        bounded_context(process.standard_error.empty() ? process.error_message : process.standard_error);
    auto message = std::format("ffprobe executable '{}' failed for source '{}' ({})",
        path_to_utf8(executable),
        path_to_utf8(source_path),
        process_exit_state_name(process.state));
    if (!detail.empty()) {
        message += ": " + std::string {detail};
    }
    if (!context.empty()) {
        message += ": " + context;
    }
    return ProbeResult {.process = std::move(process), .error_message = std::move(message)};
}

[[nodiscard]] auto parse_seconds(const Json& value) -> std::optional<u64> {
    std::string text;
    if (value.is_string()) {
        text = value.get<std::string>();
    } else if (value.is_number()) {
        text = value.dump();
    } else {
        return std::nullopt;
    }
    char* end = nullptr;
    const double seconds = std::strtod(text.c_str(), &end);
    if (end != text.c_str() + text.size() || !std::isfinite(seconds) || seconds < 0.0) {
        return std::nullopt;
    }
    return static_cast<u64>((seconds * 1000.0) + 0.5);
}

[[nodiscard]] auto parse_frame_rate(const Json& value) -> std::optional<FrameRate> {
    if (!value.is_string()) {
        return std::nullopt;
    }
    const std::string text = value.get<std::string>();
    const size_t separator = text.find('/');
    if (separator == std::string::npos) {
        return std::nullopt;
    }
    auto numerator = u32 {0};
    auto denominator = u32 {0};
    const char* const first = text.data();
    const char* const middle = first + separator;
    const char* const last = first + text.size();
    const std::from_chars_result numerator_result = std::from_chars(first, middle, numerator);
    const std::from_chars_result denominator_result = std::from_chars(middle + 1, last, denominator);
    const bool valid = numerator_result.ec == std::errc {} && numerator_result.ptr == middle
        && denominator_result.ec == std::errc {} && denominator_result.ptr == last && numerator > 0 && denominator > 0;
    return valid ? std::optional<FrameRate> {FrameRate {.numerator = numerator, .denominator = denominator}}
                 : std::nullopt;
}

[[nodiscard]] auto parse_stream(const Json& value, const size_t fallback_index) -> std::optional<StreamMetadata> {
    constexpr auto maximum_text_bytes = size_t {128};
    if (!value.is_object()) {
        return std::nullopt;
    }

    auto metadata = StreamMetadata {.index = static_cast<i32>(fallback_index)};
    if (value.contains("index")) {
        if (!value["index"].is_number_integer()) {
            return std::nullopt;
        }
        const i64 index = value["index"].get<i64>();
        if (index < 0 || index > std::numeric_limits<i32>::max()) {
            return std::nullopt;
        }
        metadata.index = static_cast<i32>(index);
    }
    if (value.contains("codec_type")) {
        if (!value["codec_type"].is_string()) {
            return std::nullopt;
        }
        metadata.codec_type = value["codec_type"].get<std::string>();
    }
    if (value.contains("codec_name")) {
        if (!value["codec_name"].is_string()) {
            return std::nullopt;
        }
        metadata.codec_name = value["codec_name"].get<std::string>();
    }
    if (metadata.codec_type.size() > maximum_text_bytes || metadata.codec_name.size() > maximum_text_bytes) {
        return std::nullopt;
    }
    return metadata;
}

[[nodiscard]] auto parse_metadata(const Json& root, const Path& source_path) -> std::optional<VideoMetadata> {
    if (!root.is_object() || !root.contains("format") || !root["format"].is_object()
        || !root["format"].contains("duration") || !root.contains("streams") || !root["streams"].is_array()) {
        return std::nullopt;
    }
    const std::optional<u64> duration = parse_seconds(root["format"]["duration"]);
    if (!duration.has_value() || *duration == 0) {
        return std::nullopt;
    }

    const Json& streams = root["streams"];
    if (streams.size() > maximum_probe_streams) {
        return std::nullopt;
    }

    auto parsed_streams = std::vector<StreamMetadata> {};
    parsed_streams.reserve(streams.size());
    auto frame_rate = std::optional<FrameRate> {};
    for (auto index = size_t {0}; index < streams.size(); ++index) {
        const Json& stream = streams[index];
        const std::optional<StreamMetadata> parsed = parse_stream(stream, index);
        if (!parsed.has_value()) {
            return std::nullopt;
        }
        parsed_streams.push_back(*parsed);
        if (frame_rate.has_value() || parsed->codec_type != "video") {
            continue;
        }
        if (stream.contains("avg_frame_rate")) {
            frame_rate = parse_frame_rate(stream["avg_frame_rate"]);
        }
        if (!frame_rate.has_value() && stream.contains("r_frame_rate")) {
            frame_rate = parse_frame_rate(stream["r_frame_rate"]);
        }
    }
    auto metadata = VideoMetadata {
        .source_path = source_path,
        .duration_ms = *duration,
        .frame_rate = frame_rate.value_or(FrameRate {}),
        .streams = std::move(parsed_streams),
        .source_extension = source_path.extension().empty() ? ".mp4" : path_to_utf8(source_path.extension()),
    };
    std::ranges::transform(metadata.source_extension, metadata.source_extension.begin(), [](const unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });

    if (!root.contains("chapters")) {
        return metadata;
    }
    if (!root["chapters"].is_array()) {
        return std::nullopt;
    }
    const Json& chapters = root["chapters"];
    metadata.embedded_chapters.reserve(chapters.size());
    for (auto index = size_t {0}; index < chapters.size(); ++index) {
        const Json& chapter = chapters[index];
        if (!chapter.is_object() || !chapter.contains("start_time") || !chapter.contains("end_time")) {
            return std::nullopt;
        }
        const std::optional<u64> start = parse_seconds(chapter["start_time"]);
        const std::optional<u64> end = parse_seconds(chapter["end_time"]);
        if (!start.has_value() || !end.has_value() || *end <= *start) {
            return std::nullopt;
        }
        auto title = std::format("Chapter {}", index + 1);
        if (chapter.contains("tags") && chapter["tags"].is_object() && chapter["tags"].contains("title")) {
            if (!chapter["tags"]["title"].is_string()) {
                return std::nullopt;
            }
            title = chapter["tags"]["title"].get<std::string>();
        }
        metadata.embedded_chapters.push_back(
            ChapterSegment {.name = std::move(title), .start_ms = *start, .end_ms = *end});
    }
    return metadata;
}

} // namespace

auto ProbeResult::ok() const noexcept -> bool {
    return success;
}

ProbeService::ProbeService(ProcessExecutor executor)
    : executor_ {std::move(executor)}
    , tool_resolver_ {ToolDiscoveryOptions {.executor = executor_}}
    , validate_tools_ {is_default_process_executor(executor_)} {
}

auto ProbeService::probe(const Path& executable, const Path& source_path, const std::stop_token stop_token) const
    -> ProbeResult {
    auto resolved_executable = executable;
    auto tool = ToolResolution {.kind = ToolKind::Ffprobe};
    if (validate_tools_) {
        tool = tool_resolver_.resolve(ToolKind::Ffprobe, executable);
        if (!tool.ok()) {
            const std::string message = std::format(
                "ffprobe tool discovery failed for source '{}': {}", path_to_utf8(source_path), tool.failure_reason);
            return ProbeResult {
                .process = ProcessResult {.state = ProcessExitState::FailedStart, .error_message = message},
                .error_message = message,
                .tool = std::move(tool),
            };
        }
        resolved_executable = tool.selected_path;
    }
    const auto request = ProcessRequest {
        .executable = resolved_executable,
        .arguments = {"-v",
            "error",
            "-print_format",
            "json",
            "-show_format",
            "-show_streams",
            "-show_chapters",
            path_to_utf8(source_path)},
        .stop_token = stop_token,
    };
    ProbeResult result = parse_probe_output(resolved_executable, source_path, executor_(request));
    if (validate_tools_) {
        result.tool = std::move(tool);
    }
    return result;
}

auto parse_probe_output(const Path& executable, const Path& source_path, ProcessResult process) -> ProbeResult {
    if (!process.ok()) {
        return failure(executable, source_path, std::move(process), "process execution failed");
    }

    Json root;
    try {
        root = Json::parse(process.standard_output);
    } catch (const Json::parse_error& error) {
        return failure(executable, source_path, std::move(process), error.what());
    }
    const std::optional<VideoMetadata> metadata = parse_metadata(root, source_path);
    if (!metadata.has_value()) {
        return failure(executable, source_path, std::move(process), "unsupported or missing ffprobe metadata");
    }
    return ProbeResult {.success = true, .metadata = *metadata, .process = std::move(process)};
}

} // namespace vidchopper
