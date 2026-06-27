#include "core/timecode.h"

#include "core/string_utils.h"

#include <charconv>
#include <cmath>
#include <format>
#include <system_error>
#include <vector>

namespace vidchopper {

namespace {

auto split_timecode(std::string_view value, const char delimiter) -> std::vector<std::string> {
    auto segments = std::vector<std::string> {};
    auto current = std::string {};

    for (const auto character : value) {
        if (character == delimiter) {
            segments.push_back(current);
            current.clear();
            continue;
        }

        current.push_back(character);
    }

    segments.push_back(current);
    return segments;
}

auto parse_unsigned(std::string_view value) -> std::optional<u64> {
    auto result = u64 {0};
    const auto* const first = value.data();
    const auto* const last = first + value.size();
    const auto [parse_end, error] = std::from_chars(first, last, result);
    if (error != std::errc {} || parse_end != last) {
        return std::nullopt;
    }

    return result;
}

struct HmsComponents {
    u64 hours;
    u64 minutes;
    u64 seconds;
};

constexpr auto decompose_total_seconds(const u64 total_seconds) noexcept -> HmsComponents {
    return HmsComponents {
        .hours = total_seconds / 3600,
        .minutes = (total_seconds / 60) % 60,
        .seconds = total_seconds % 60,
    };
}

struct ParsedHmsSegments {
    u64 hours;
    u64 minutes;
    u64 seconds;
    std::string trailing_segment;
};

auto parse_hms_segments(std::string_view value, const usize min_segments, const usize max_segments) -> std::optional<ParsedHmsSegments> {
    const auto trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    const auto segments = split_timecode(trimmed, ':');
    if (segments.size() < min_segments || segments.size() > max_segments) {
        return std::nullopt;
    }

    const auto has_hours = segments.size() == max_segments;
    const auto hours = has_hours ? parse_unsigned(segments[0]) : std::optional<u64> {0};
    const auto minutes = parse_unsigned(segments[has_hours ? 1 : 0]);
    const auto seconds_index = has_hours ? 2 : 1;

    if (!hours.has_value() || !minutes.has_value()) {
        return std::nullopt;
    }

    if (*minutes >= 60) {
        return std::nullopt;
    }

    const auto seconds_str = (static_cast<usize>(seconds_index) < segments.size() - 1)
        ? segments[seconds_index]
        : std::string {};

    const auto& trailing = segments.back();

    if (seconds_str.empty()) {
        return ParsedHmsSegments {
            .hours = *hours,
            .minutes = *minutes,
            .seconds = 0,
            .trailing_segment = trailing,
        };
    }

    const auto seconds = parse_unsigned(seconds_str);
    if (!seconds.has_value() || *seconds >= 60) {
        return std::nullopt;
    }

    return ParsedHmsSegments {
        .hours = *hours,
        .minutes = *minutes,
        .seconds = *seconds,
        .trailing_segment = trailing,
    };
}

} // namespace

auto parse_millisecond_timecode(std::string_view value) -> std::optional<u64> {
    const auto trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    const auto segments = split_timecode(trimmed, ':');
    if (segments.size() < 2 || segments.size() > 3) {
        return std::nullopt;
    }

    const auto hours = segments.size() == 3 ? parse_unsigned(segments[0]) : std::optional<u64> {0};
    const auto minutes = parse_unsigned(segments[segments.size() == 3 ? 1 : 0]);

    const auto second_parts = split_timecode(segments.back(), '.');
    if (second_parts.empty() || second_parts.size() > 2) {
        return std::nullopt;
    }

    const auto seconds = parse_unsigned(second_parts[0]);
    if (!hours.has_value() || !minutes.has_value() || !seconds.has_value()) {
        return std::nullopt;
    }

    auto milliseconds = u64 {0};
    if (second_parts.size() == 2) {
        const auto fraction = second_parts[1];
        if (fraction.empty() || fraction.size() > 3) {
            return std::nullopt;
        }

        const auto parsed_fraction = parse_unsigned(fraction);
        if (!parsed_fraction.has_value()) {
            return std::nullopt;
        }

        milliseconds = *parsed_fraction;
        if (fraction.size() == 1) {
            milliseconds *= 100;
        } else if (fraction.size() == 2) {
            milliseconds *= 10;
        }
    }

    if (*minutes >= 60 || *seconds >= 60) {
        return std::nullopt;
    }

    return (((*hours * 60) + *minutes) * 60 * 1000) + (*seconds * 1000) + milliseconds;
}

auto parse_frame_timecode(std::string_view value, const FrameRate& frame_rate) -> std::optional<u64> {
    const auto fps = frame_rate.display_frames_per_second();
    if (fps == 0) {
        return std::nullopt;
    }

    const auto parsed = parse_hms_segments(value, 3, 4);
    if (!parsed.has_value()) {
        return std::nullopt;
    }

    const auto frames = parse_unsigned(parsed->trailing_segment);
    if (!frames.has_value() || *frames >= fps) {
        return std::nullopt;
    }

    if (parsed->seconds >= 60) {
        return std::nullopt;
    }

    const auto base_seconds = ((parsed->hours * 60) + parsed->minutes) * 60 + parsed->seconds;
    const auto total_frames = (base_seconds * fps) + *frames;
    const auto milliseconds = static_cast<u64>(std::llround((static_cast<f64>(total_frames) * 1000.0) / static_cast<f64>(fps)));

    return milliseconds;
}

auto format_millisecond_timecode(const u64 milliseconds) -> std::string {
    const auto hms = decompose_total_seconds(milliseconds / 1000);
    const auto remaining_ms = milliseconds % 1000;

    return std::format("{:02}:{:02}:{:02}.{:03}", hms.hours, hms.minutes, hms.seconds, remaining_ms);
}

auto format_frame_timecode(const u64 milliseconds, const FrameRate& frame_rate) -> std::string {
    const auto fps = frame_rate.display_frames_per_second();
    if (fps == 0) {
        return "00:00:00:00";
    }

    const auto total_frames = static_cast<u64>(std::llround((static_cast<f64>(milliseconds) * static_cast<f64>(fps)) / 1000.0));
    const auto frames = total_frames % fps;
    const auto hms = decompose_total_seconds(total_frames / fps);

    return std::format("{:02}:{:02}:{:02}:{:02}", hms.hours, hms.minutes, hms.seconds, frames);
}

} // namespace vidchopper
