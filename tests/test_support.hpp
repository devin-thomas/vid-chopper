#pragma once

#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace test_support {

inline auto install_terminate_diagnostics() -> void {
    std::set_terminate([]() noexcept {
        const std::exception_ptr exception = std::current_exception();
        if (exception != nullptr) {
            try {
                std::rethrow_exception(exception);
            } catch (const std::exception& error) {
                std::cerr << "Unhandled test exception: " << error.what() << '\n';
            } catch (...) {
                std::cerr << "Unhandled non-standard test exception.\n";
            }
        } else {
            std::cerr << "Test terminated without an active exception.\n";
        }
        std::abort();
    });
}

struct TerminateDiagnosticsInstaller final {
    TerminateDiagnosticsInstaller() {
        install_terminate_diagnostics();
    }
};

inline const TerminateDiagnosticsInstaller terminate_diagnostics_installer {};

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
