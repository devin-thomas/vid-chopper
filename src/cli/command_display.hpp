#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace vidchopper {

[[nodiscard]] auto quote_command_argument(std::string_view argument) -> std::string;
[[nodiscard]] auto display_command(const std::vector<std::string>& command) -> std::string;

} // namespace vidchopper
