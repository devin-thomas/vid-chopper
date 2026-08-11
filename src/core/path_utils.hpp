#pragma once

#include "core/types.hpp"

#include <filesystem>
#include <string>
#include <string_view>

namespace vidchopper {

[[nodiscard]] inline auto path_to_utf8(const Path& path) -> std::string {
    const auto encoded = path.u8string();
    auto result = std::string {};
    result.reserve(encoded.size());
    for (const char8_t character : encoded) {
        result.push_back(static_cast<char>(character));
    }
    return result;
}

[[nodiscard]] inline auto path_from_utf8(const std::string_view text) -> Path {
    auto encoded = std::u8string {};
    encoded.reserve(text.size());
    for (const char character : text) {
        encoded.push_back(static_cast<char8_t>(character));
    }
    return Path {encoded};
}

} // namespace vidchopper
