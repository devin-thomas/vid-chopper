#pragma once

#include "core/models.h"

#include <optional>
#include <string>
#include <string_view>

namespace vidchopper {

[[nodiscard]] auto parse_millisecond_timecode(std::string_view value) -> std::optional<u64>;
[[nodiscard]] auto parse_frame_timecode(std::string_view value, const FrameRate& frame_rate) -> std::optional<u64>;
[[nodiscard]] auto format_millisecond_timecode(u64 milliseconds) -> std::string;
[[nodiscard]] auto format_frame_timecode(u64 milliseconds, const FrameRate& frame_rate) -> std::string;

} // namespace vidchopper
