#include "services/process_runner.hpp"
#include "test_support.hpp"

#define NOMINMAX
#include <Windows.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stop_token>
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
        if (mode == "--echo-argument" && argument_count == 3) {
            std::cout << arguments[2];
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
    auto streamed_stdout = std::string {};
    auto streamed_stderr = std::string {};
    const ProcessResult success = run_process(ProcessRequest {
        .executable = self,
        .arguments = {"--emit"},
        .standard_output_chunk = [&streamed_stdout](const std::string_view chunk) { streamed_stdout.append(chunk); },
        .standard_error_chunk = [&streamed_stderr](const std::string_view chunk) { streamed_stderr.append(chunk); },
    });
    test_support::expect_eq(success.state, ProcessExitState::Success, "successful child should be distinguished");
    test_support::expect_eq(success.standard_output, std::string {"standard output"}, "stdout should be captured");
    test_support::expect_eq(success.standard_error, std::string {"standard error"}, "stderr should be captured");
    test_support::expect_eq(
        streamed_stdout, success.standard_output, "bounded stdout chunks should be available while capturing");
    test_support::expect_eq(
        streamed_stderr, success.standard_error, "bounded stderr chunks should be available while capturing");

    const Path missing_executable = self.parent_path() / "missing.exe";
    const ProcessResult missing = run_process(ProcessRequest {.executable = missing_executable});
    test_support::expect_eq(
        missing.state, ProcessExitState::FailedStart, "missing executable should be a failed start, not a timeout");

    auto prelaunch_stop = std::stop_source {};
    prelaunch_stop.request_stop();
    const ProcessResult cancelled_before_start = run_process(ProcessRequest {
        .executable = missing_executable,
        .stop_token = prelaunch_stop.get_token(),
    });
    test_support::expect_eq(cancelled_before_start.state,
        ProcessExitState::Cancelled,
        "pre-launch cancellation should take precedence over starting a child");

    const std::wstring copied_directory_name =
        std::wstring {L"vidchopper process runner \u89C6\u9891-"} + std::to_wstring(GetCurrentProcessId());
    const Path copied_directory = std::filesystem::temp_directory_path() / copied_directory_name;
    const Path copied_self = copied_directory / "test child.exe";
    static_cast<void>(std::filesystem::remove_all(copied_directory));
    static_cast<void>(std::filesystem::create_directories(copied_directory));
    const bool copied =
        std::filesystem::copy_file(self, copied_self, std::filesystem::copy_options::overwrite_existing);
    test_support::expect_true(copied, "process runner fixture should copy into a spaced Unicode path");
    const auto spaced_argument = std::string {R"(C:\match clips\set.mkv)"};
    const ProcessResult quoted =
        run_process(ProcessRequest {.executable = copied_self, .arguments = {"--echo-argument", spaced_argument}});
    test_support::expect_eq(quoted.state, ProcessExitState::Success, "a spaced Unicode executable path should start");
    test_support::expect_eq(quoted.standard_output, spaced_argument, "a spaced process argument should round-trip");

    const ProcessResult timeout = run_process(ProcessRequest {
        .executable = self,
        .arguments = {"--sleep"},
        .timeout = std::chrono::milliseconds {50},
    });
    test_support::expect_eq(timeout.state, ProcessExitState::TimedOut, "long-running child should time out");

    const ProcessResult crash = run_process(ProcessRequest {.executable = self, .arguments = {"--crash"}});
    test_support::expect_eq(crash.state, ProcessExitState::Crashed, "aborted child should be reported as a crash");
    test_support::expect_eq(crash.exit_code, i32 {0}, "a crash should not expose an ordinary process exit code");

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

    auto stop_source = std::stop_source {};
    auto cancelled = ProcessResult {};
    auto cancellation_thread = std::thread {[&cancelled, &stop_source, &self]() {
        cancelled = run_process(ProcessRequest {
            .executable = self,
            .arguments = {"--sleep"},
            .timeout = std::chrono::seconds {5},
            .stop_token = stop_source.get_token(),
        });
    }};
    std::this_thread::sleep_for(std::chrono::milliseconds {50});
    stop_source.request_stop();
    cancellation_thread.join();
    test_support::expect_eq(
        cancelled.state, ProcessExitState::Cancelled, "a requested stop should terminate the child as cancelled");

    static_cast<void>(std::filesystem::remove_all(copied_directory));
    return 0;
}
