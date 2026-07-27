#include "cli/process_runner.hpp"
#include "test_support.hpp"

#define NOMINMAX
#include <Windows.h>

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using namespace vidchopper;

namespace {

[[nodiscard]] auto executable_path() -> Path {
    auto buffer = std::wstring(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return Path {buffer};
}

} // namespace

auto main(const int argument_count, char** arguments) -> int {
    if (argument_count > 1) {
        const std::string mode = arguments[1];
        if (mode == "--emit") {
            std::cout << "standard output";
            std::cerr << "standard error";
            return 0;
        }
        if (mode == "--sleep") {
            std::this_thread::sleep_for(std::chrono::seconds {2});
            return 0;
        }
        if (mode == "--crash") {
            std::abort();
        }
        if (mode == "--exit-7") {
            return 7;
        }
        if (mode == "--stderr-large") {
            std::cerr << std::string(10000, 'x');
            return 9;
        }
    }

    const Path self = executable_path();
    const ProcessResult success = run_process(ProcessRequest {.executable = self, .arguments = {"--emit"}});
    test_support::expect_eq(success.state, ProcessExitState::Success, "successful child should be distinguished");
    test_support::expect_eq(success.standard_output, std::string {"standard output"}, "stdout should be captured");
    test_support::expect_eq(success.standard_error, std::string {"standard error"}, "stderr should be captured");

    const ProcessResult missing = run_process(ProcessRequest {.executable = self.parent_path() / "missing.exe"});
    test_support::expect_eq(
        missing.state, ProcessExitState::FailedStart, "missing executable should be a failed start, not a timeout");

    const ProcessResult timeout = run_process(ProcessRequest {
        .executable = self,
        .arguments = {"--sleep"},
        .timeout = std::chrono::milliseconds {50},
    });
    test_support::expect_eq(timeout.state, ProcessExitState::TimedOut, "long-running child should time out");

    const ProcessResult crash = run_process(ProcessRequest {.executable = self, .arguments = {"--crash"}});
    test_support::expect_eq(crash.state, ProcessExitState::Crashed, "aborted child should be reported as a crash");

    const ProcessResult nonzero = run_process(ProcessRequest {.executable = self, .arguments = {"--exit-7"}});
    test_support::expect_eq(nonzero.state, ProcessExitState::NonzeroExit, "ordinary failure should be nonzero exit");
    test_support::expect_eq(nonzero.exit_code, i32 {7}, "nonzero exit code should be preserved");

    const ProcessResult bounded = run_process(ProcessRequest {
        .executable = self,
        .arguments = {"--stderr-large"},
        .stderr_limit_bytes = 32,
    });
    test_support::expect_eq(bounded.state, ProcessExitState::NonzeroExit, "bounded stderr child should fail normally");
    test_support::expect_eq(bounded.standard_error.size(), size_t {32}, "captured stderr should honor its bound");

    return 0;
}
