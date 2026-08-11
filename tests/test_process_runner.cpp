#include "services/process_runner.hpp"
#include "test_support.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace vidchopper;

namespace {

[[nodiscard]] auto executable_path() -> Path {
#ifdef _WIN32
    auto buffer = std::wstring(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return Path {buffer};
#elif defined(__APPLE__)
    auto size = uint32_t {0};
    static_cast<void>(_NSGetExecutablePath(nullptr, &size));
    auto buffer = std::vector<char>(size);
    const int status = _NSGetExecutablePath(buffer.data(), &size);
    test_support::expect_eq(status, 0, "test executable path should be available");
    return Path {buffer.data()};
#else
    auto buffer = std::array<char, 4096> {};
    const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    test_support::expect_true(length > 0, "test executable path should be available");
    buffer[static_cast<size_t>(length)] = '\0';
    return Path {buffer.data()};
#endif
}

[[nodiscard]] auto contains(const std::string_view text, const std::string_view needle) -> bool {
    return text.find(needle) != std::string_view::npos;
}

#ifndef _WIN32
[[nodiscard]] auto open_descriptor_count() -> size_t {
    size_t count = 0;
    for (int descriptor = 0; descriptor < ::getdtablesize(); ++descriptor) {
        if (::fcntl(descriptor, F_GETFD) >= 0) {
            ++count;
        }
    }
    return count;
}

[[nodiscard]] auto read_pid(const Path& path) -> pid_t {
    auto stream = std::ifstream {path};
    auto process_id = pid_t {0};
    stream >> process_id;
    return process_id;
}

[[nodiscard]] auto wait_for_process_exit(const pid_t process_id) -> bool {
    for (auto attempt = 0; attempt < 100; ++attempt) {
        if (::kill(process_id, 0) != 0 && errno == ESRCH) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds {10});
    }
    return false;
}
#endif

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
#ifndef _WIN32
        if (mode == "--both-large") {
            std::cout << std::string(512 * 1024, 'o');
            std::cerr << std::string(512 * 1024, 'e');
            return 9;
        }
        if (mode == "--stdout-close") {
            static_cast<void>(::close(STDOUT_FILENO));
            std::cerr << "stderr after stdout close";
            return 0;
        }
        if (mode == "--signal") {
            static_cast<void>(std::raise(SIGTERM));
        }
        if (mode == "--spawn-descendant" && argument_count == 3) {
            const pid_t descendant = ::fork();
            test_support::expect_true(descendant >= 0, "descendant fixture should fork");
            if (descendant == 0) {
                static_cast<void>(std::signal(SIGTERM, SIG_IGN));
                auto pid_file = std::ofstream {arguments[2]};
                pid_file << ::getpid() << '\n';
                pid_file.flush();
                while (true) {
                    pause();
                }
            }
            std::this_thread::sleep_for(std::chrono::seconds {2});
            return 0;
        }
#endif
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
    test_support::expect_true(contains(missing.error_message, "execute process"), "failed start should explain exec");

    auto prelaunch_stop = std::stop_source {};
    prelaunch_stop.request_stop();
    const ProcessResult cancelled_before_start = run_process(ProcessRequest {
        .executable = missing_executable,
        .stop_token = prelaunch_stop.get_token(),
    });
    test_support::expect_eq(cancelled_before_start.state,
        ProcessExitState::Cancelled,
        "pre-launch cancellation should take precedence over starting a child");

#ifdef _WIN32
    const std::wstring copied_directory_name =
        std::wstring {L"vidchopper process runner \u89C6\u9891-"} + std::to_wstring(GetCurrentProcessId());
    const Path copied_directory = std::filesystem::temp_directory_path() / copied_directory_name;
    const Path copied_self = copied_directory / "test child.exe";
#else
    const Path copied_directory =
        std::filesystem::temp_directory_path() / ("vidchopper process runner " + std::to_string(::getpid()));
    const Path copied_self = copied_directory / "test child";
#endif
    static_cast<void>(std::filesystem::remove_all(copied_directory));
    static_cast<void>(std::filesystem::create_directories(copied_directory));
    const bool copied =
        std::filesystem::copy_file(self, copied_self, std::filesystem::copy_options::overwrite_existing);
    test_support::expect_true(copied, "process runner fixture should copy into a spaced path");
#ifndef _WIN32
    static_cast<void>(std::filesystem::permissions(
        copied_self, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add));
#endif
#ifdef _WIN32
    const auto spaced_argument = std::string {R"(C:\match clips\set.mkv)"};
#else
    const auto spaced_argument = std::string {"path with spaces \"quotes\" \\\\ "} + "\xE6\x96\x87";
#endif
    const ProcessResult quoted =
        run_process(ProcessRequest {.executable = copied_self, .arguments = {"--echo-argument", spaced_argument}});
    test_support::expect_eq(quoted.state, ProcessExitState::Success, "a spaced executable path should start");
    test_support::expect_eq(quoted.standard_output, spaced_argument, "a complex process argument should round-trip");

    const ProcessResult timeout = run_process(ProcessRequest {
        .executable = self,
        .arguments = {"--sleep"},
        .timeout = std::chrono::milliseconds {50},
    });
    test_support::expect_eq(timeout.state, ProcessExitState::TimedOut, "long-running child should time out");

#ifndef _WIN32
    const ProcessResult signaled = run_process(ProcessRequest {.executable = self, .arguments = {"--signal"}});
    test_support::expect_eq(signaled.state, ProcessExitState::Signaled, "signal termination should be distinct");
    test_support::expect_eq(signaled.termination_signal, i32 {SIGTERM}, "signal information should be retained");
    test_support::expect_true(contains(signaled.error_message, "signal"), "signal diagnostics should name the signal");
#endif

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

#ifndef _WIN32
    const size_t descriptor_count_before = open_descriptor_count();
    const ProcessResult both_large = run_process(ProcessRequest {
        .executable = self,
        .arguments = {"--both-large"},
        .stdout_limit_bytes = 128,
        .stderr_limit_bytes = 128,
    });
    test_support::expect_eq(both_large.state, ProcessExitState::NonzeroExit, "large dual-stream child should finish");
    test_support::expect_eq(both_large.standard_output.size(), size_t {128}, "stdout should remain bounded");
    test_support::expect_eq(both_large.standard_error.size(), size_t {128}, "stderr should remain bounded");

    const ProcessResult early_close = run_process(ProcessRequest {
        .executable = self,
        .arguments = {"--stdout-close"},
    });
    test_support::expect_eq(early_close.state, ProcessExitState::Success, "early stream close should not deadlock");
    test_support::expect_eq(
        early_close.standard_error, std::string {"stderr after stdout close"}, "stderr should survive stdout close");

    for (auto attempt = 0; attempt < 4; ++attempt) {
        const ProcessResult descriptor_check = run_process(ProcessRequest {
            .executable = self,
            .arguments = {"--emit"},
        });
        test_support::expect_eq(
            descriptor_check.state, ProcessExitState::Success, "descriptor cleanup fixture should complete");
    }
    test_support::expect_eq(open_descriptor_count(),
        descriptor_count_before,
        "repeated process runs should close all parent-side descriptors");

    const Path descendant_pid_file = copied_directory / "descendant.pid";
    const ProcessResult descendant = run_process(ProcessRequest {
        .executable = self,
        .arguments = {"--spawn-descendant", descendant_pid_file.string()},
        .timeout = std::chrono::milliseconds {250},
    });
    test_support::expect_eq(descendant.state, ProcessExitState::TimedOut, "timeout should terminate the process group");
    auto descendant_pid = pid_t {0};
    for (auto attempt = 0; attempt < 50 && descendant_pid == 0; ++attempt) {
        descendant_pid = read_pid(descendant_pid_file);
        if (descendant_pid == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }
    }
    test_support::expect_true(descendant_pid > 0, "descendant fixture should report its pid");
    test_support::expect_true(
        wait_for_process_exit(descendant_pid), "timeout should not leave a process-group descendant running");

    const Path cancellation_pid_file = copied_directory / "cancellation-descendant.pid";
    auto descendant_stop_source = std::stop_source {};
    auto cancelled_descendant = ProcessResult {};
    auto descendant_cancellation_thread =
        std::thread {[&cancelled_descendant, &descendant_stop_source, &self, &cancellation_pid_file]() {
            cancelled_descendant = run_process(ProcessRequest {
                .executable = self,
                .arguments = {"--spawn-descendant", cancellation_pid_file.string()},
                .timeout = std::chrono::seconds {5},
                .stop_token = descendant_stop_source.get_token(),
            });
        }};
    std::this_thread::sleep_for(std::chrono::milliseconds {50});
    descendant_stop_source.request_stop();
    descendant_cancellation_thread.join();
    test_support::expect_eq(cancelled_descendant.state,
        ProcessExitState::Cancelled,
        "cancellation should terminate the complete process group");
    auto cancellation_pid = pid_t {0};
    for (auto attempt = 0; attempt < 50 && cancellation_pid == 0; ++attempt) {
        cancellation_pid = read_pid(cancellation_pid_file);
        if (cancellation_pid == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds {10});
        }
    }
    test_support::expect_true(cancellation_pid > 0, "cancelled descendant fixture should report its pid");
    test_support::expect_true(
        wait_for_process_exit(cancellation_pid), "cancellation should not leave a process-group descendant running");
#endif

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
