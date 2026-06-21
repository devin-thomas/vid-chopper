#include "core/timecode.h"

#include "core/string_utils.h"

#include <cmath>
#include <iomanip>
#include <sstream>
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
    if (value.empty()) {
        return std::nullopt;
    }

    auto result = u64 {0};
    for (const auto character : value) {
        if (character < '0' || character > '9') {
            return std::nullopt;
        }

        result = (result * 10) + static_cast<u64>(character - '0');
    }

    return result;
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

    const auto trimmed = trim_copy(value);
    const auto segments = split_timecode(trimmed, ':');
    if (segments.size() < 3 || segments.size() > 4) {
        return std::nullopt;
    }

    const auto hours = segments.size() == 4 ? parse_unsigned(segments[0]) : std::optional<u64> {0};
    const auto minutes = parse_unsigned(segments[segments.size() == 4 ? 1 : 0]);
    const auto seconds = parse_unsigned(segments[segments.size() == 4 ? 2 : 1]);
    const auto frames = parse_unsigned(segments.back());

    if (!hours.has_value() || !minutes.has_value() || !seconds.has_value() || !frames.has_value()) {
        return std::nullopt;
    }

    if (*minutes >= 60 || *seconds >= 60 || *frames >= fps) {
        return std::nullopt;
    }

    const auto base_seconds = ((*hours * 60) + *minutes) * 60 + *seconds;
    const auto total_frames = (base_seconds * fps) + *frames;
    const auto milliseconds = static_cast<u64>(std::llround((static_cast<f64>(total_frames) * 1000.0) / static_cast<f64>(fps)));

    return milliseconds;
}

auto format_millisecond_timecode(const u64 milliseconds) -> std::string {
    const auto total_seconds = milliseconds / 1000;
    const auto remaining_ms = milliseconds % 1000;
    const auto seconds = total_seconds % 60;
    const auto minutes = (total_seconds / 60) % 60;
    const auto hours = total_seconds / 3600;

    auto builder = std::ostringstream {};
    builder << std::setw(2) << std::setfill('0') << hours << ':'
            << std::setw(2) << std::setfill('0') << minutes << ':'
            << std::setw(2) << std::setfill('0') << seconds << '.'
            << std::setw(3) << std::setfill('0') << remaining_ms;
    return builder.str();
}

auto format_frame_timecode(const u64 milliseconds, const FrameRate& frame_rate) -> std::string {
    const auto fps = frame_rate.display_frames_per_second();
    if (fps == 0) {
        return "00:00:00:00";
    }

    const auto total_frames = static_cast<u64>(std::llround((static_cast<f64>(milliseconds) * static_cast<f64>(fps)) / 1000.0));
    const auto frames = total_frames % fps;
    const auto total_seconds = total_frames / fps;
    const auto seconds = total_seconds % 60;
    const auto minutes = (total_seconds / 60) % 60;
    const auto hours = total_seconds / 3600;

    auto builder = std::ostringstream {};
    builder << std::setw(2) << std::setfill('0') << hours << ':'
            << std::setw(2) << std::setfill('0') << minutes << ':'
            << std::setw(2) << std::setfill('0') << seconds << ':'
            << std::setw(2) << std::setfill('0') << frames;
    return builder.str();
}

} // namespace vidchopper
