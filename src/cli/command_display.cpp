#include "cli/command_display.hpp"

namespace vidchopper {

auto quote_command_argument(const std::string_view argument) -> std::string {
    auto quoted = std::string {"\""};
    auto backslash_count = size_t {0};
    for (const char character : argument) {
        if (character == '\\') {
            ++backslash_count;
            continue;
        }
        if (character == '\"') {
            quoted.append(backslash_count * 2 + 1, '\\');
            quoted.push_back(character);
        } else {
            quoted.append(backslash_count, '\\');
            quoted.push_back(character);
        }
        backslash_count = 0;
    }
    quoted.append(backslash_count * 2, '\\');
    quoted.push_back('\"');
    return quoted;
}

auto display_command(const std::vector<std::string>& command) -> std::string {
    auto result = std::string {};
    for (const std::string& argument : command) {
        if (!result.empty()) {
            result.push_back(' ');
        }
        const bool needs_quotes = argument.find_first_of(" \t\"") != std::string::npos;
        result += needs_quotes ? quote_command_argument(argument) : argument;
    }
    return result;
}

} // namespace vidchopper
