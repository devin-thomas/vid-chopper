#include "core/string_utils.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace vidchopper {

auto trim_copy(std::string_view value) -> std::string {
    auto start = usize {0};
    auto end = value.size();

    while (start < end && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return std::string {value.substr(start, end - start)};
}

auto replace_all_copy(std::string value, std::string_view from, std::string_view to) -> std::string {
    if (from.empty()) {
        return value;
    }

    auto position = usize {0};
    while ((position = value.find(from, position)) != std::string::npos) {
        value.replace(position, from.size(), to);
        position += to.size();
    }

    return value;
}

auto split_quoted_arguments(std::string_view value) -> std::vector<std::string> {
    auto tokens = std::vector<std::string> {};
    auto current = std::string {};
    auto in_quotes = false;

    for (const auto character : value) {
        if (character == '"') {
            in_quotes = !in_quotes;
            continue;
        }

        if (!in_quotes && std::isspace(static_cast<unsigned char>(character)) != 0) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }

            continue;
        }

        current.push_back(character);
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}

auto sanitize_file_component(std::string_view value) -> std::string {
    constexpr auto forbidden = std::string_view {"<>:\"/\\|?*"};

    auto sanitized = std::string {};
    sanitized.reserve(value.size());

    auto previous_was_space = false;

    for (const auto raw_character : value) {
        if (static_cast<unsigned char>(raw_character) < 32) {
            continue;
        }

        auto character = raw_character;
        if (forbidden.find(character) != std::string::npos) {
            character = '_';
        }

        if (std::isspace(static_cast<unsigned char>(character)) != 0) {
            if (previous_was_space) {
                continue;
            }

            sanitized.push_back(' ');
            previous_was_space = true;
            continue;
        }

        previous_was_space = false;
        sanitized.push_back(character);
    }

    sanitized = trim_copy(sanitized);
    if (sanitized.empty()) {
        return "chapter";
    }

    while (!sanitized.empty() && (sanitized.back() == '.' || sanitized.back() == ' ')) {
        sanitized.pop_back();
    }

    if (sanitized.empty() || sanitized == "." || sanitized == "..") {
        return "chapter";
    }

    return sanitized;
}

auto zero_padded_index(const u16 index, const u8 width) -> std::string {
    auto builder = std::ostringstream {};
    builder << std::setw(width) << std::setfill('0') << index;
    return builder.str();
}

} // namespace vidchopper
