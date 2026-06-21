#pragma once

#include <stdexcept>
#include <string>
#include <string_view>

namespace test_support {

inline auto fail(std::string_view message) -> void {
    throw std::runtime_error(std::string {message});
}

inline auto expect_true(const bool condition, std::string_view message) -> void {
    if (!condition) {
        fail(message);
    }
}

template <typename T, typename U>
auto expect_eq(const T& actual, const U& expected, std::string_view message) -> void {
    if (!(actual == expected)) {
        fail(std::string {message});
    }
}

} // namespace test_support
