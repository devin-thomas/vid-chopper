#pragma once

#include "core/types.h"

#include <string>
#include <string_view>
#include <vector>

namespace vidchopper {

[[nodiscard]] auto trim_copy(std::string_view value) -> std::string;
[[nodiscard]] auto replace_all_copy(std::string value, std::string_view from, std::string_view to) -> std::string;
[[nodiscard]] auto split_quoted_arguments(std::string_view value) -> std::vector<std::string>;
[[nodiscard]] auto sanitize_file_component(std::string_view value) -> std::string;
[[nodiscard]] auto zero_padded_index(u16 index, u8 width) -> std::string;

} // namespace vidchopper
