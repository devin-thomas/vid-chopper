#include "services/process_runner.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <functional>
#include <limits>
#include <string_view>
#include <thread>

namespace vidchopper {

namespace {

#ifdef _WIN32

class Handle final {
public:
    Handle() = default;
    explicit Handle(HANDLE value)
        : value_ {value} {
    }
    Handle(const Handle&) = delete;
    auto operator=(const Handle&) -> Handle& = delete;
    Handle(Handle&& other) noexcept
        : value_ {other.release()} {
    }
    auto operator=(Handle&& other) noexcept -> Handle& {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }
    ~Handle() {
        reset();
    }

    [[nodiscard]] auto get() const noexcept -> HANDLE {
        return value_;
    }

    [[nodiscard]] auto valid() const noexcept -> bool {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }

    auto reset(HANDLE value = nullptr) noexcept -> void {
        if (valid()) {
            CloseHandle(value_);
        }
        value_ = value;
    }

    [[nodiscard]] auto release() noexcept -> HANDLE {
        const HANDLE value = value_;
        value_ = nullptr;
        return value;
    }

private:
    HANDLE value_ {nullptr};
};

[[nodiscard]] auto utf8_to_wide(const std::string_view text) -> std::wstring {
    if (text.empty()) {
        return {};
    }
    const int length =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    auto wide = std::wstring(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), wide.data(), length);
    return wide;
}

[[nodiscard]] auto quote_windows_argument(const std::wstring_view argument) -> std::wstring {
    auto quoted = std::wstring {L"\""};
    auto backslash_count = size_t {0};
    for (const wchar_t character : argument) {
        if (character == L'\\') {
            ++backslash_count;
            continue;
        }
        if (character == L'"') {
            quoted.append(backslash_count * 2 + 1, L'\\');
            quoted.push_back(character);
        } else {
            quoted.append(backslash_count, L'\\');
            quoted.push_back(character);
        }
        backslash_count = 0;
    }
    quoted.append(backslash_count * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

[[nodiscard]] auto command_line(const ProcessRequest& request) -> std::wstring {
    auto result = quote_windows_argument(request.executable.wstring());
    for (const std::string& argument : request.arguments) {
        result.push_back(L' ');
        result += quote_windows_argument(utf8_to_wide(argument));
    }
    return result;
}

[[nodiscard]] auto windows_error_message(const DWORD error_code) -> std::string {
    wchar_t* buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD length = FormatMessageW(flags,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<wchar_t*>(&buffer),
        0,
        nullptr);
    if (length == 0 || buffer == nullptr) {
        return std::format("Windows error {}", error_code);
    }
    const std::wstring_view wide {buffer, length};
    const int utf8_length =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    auto message = std::string(static_cast<size_t>((std::max)(utf8_length, 0)), '\0');
    if (utf8_length > 0) {
        WideCharToMultiByte(
            CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), message.data(), utf8_length, nullptr, nullptr);
    }
    LocalFree(buffer);
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' ')) {
        message.pop_back();
    }
    return message;
}

auto read_pipe(const HANDLE pipe, std::string& output, const size_t limit) -> void {
    auto buffer = std::array<char, 4096> {};
    auto bytes_read = DWORD {0};
    while (ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr) != FALSE) {
        const size_t remaining = output.size() < limit ? limit - output.size() : 0;
        const size_t count = (std::min)(remaining, static_cast<size_t>(bytes_read));
        output.append(buffer.data(), count);
    }
}

[[nodiscard]] auto create_pipe(Handle& read_end, Handle& write_end) -> bool {
    auto security = SECURITY_ATTRIBUTES {
        .nLength = sizeof(SECURITY_ATTRIBUTES),
        .lpSecurityDescriptor = nullptr,
        .bInheritHandle = TRUE,
    };
    HANDLE raw_read = nullptr;
    HANDLE raw_write = nullptr;
    if (CreatePipe(&raw_read, &raw_write, &security, 0) == FALSE) {
        return false;
    }
    read_end.reset(raw_read);
    write_end.reset(raw_write);
    return SetHandleInformation(read_end.get(), HANDLE_FLAG_INHERIT, 0) != FALSE;
}

#endif

} // namespace

auto ProcessResult::ok() const noexcept -> bool {
    return state == ProcessExitState::Success;
}

auto process_exit_state_name(const ProcessExitState state) -> std::string {
    switch (state) {
    case ProcessExitState::Success:
        return "success";
    case ProcessExitState::FailedStart:
        return "failed start";
    case ProcessExitState::TimedOut:
        return "timeout";
    case ProcessExitState::Crashed:
        return "crash";
    case ProcessExitState::NonzeroExit:
        return "nonzero exit";
    case ProcessExitState::Cancelled:
        return "cancelled";
    }
    return "unknown";
}

auto run_process(const ProcessRequest& request) -> ProcessResult {
    if (request.stop_token.stop_requested()) {
        return ProcessResult {
            .state = ProcessExitState::Cancelled,
            .error_message = "Process cancellation was requested before start.",
        };
    }

#ifdef _WIN32
    auto stdout_read = Handle {};
    auto stdout_write = Handle {};
    auto stderr_read = Handle {};
    auto stderr_write = Handle {};
    if (!create_pipe(stdout_read, stdout_write) || !create_pipe(stderr_read, stderr_write)) {
        const DWORD error_code = GetLastError();
        return ProcessResult {.error_message = windows_error_message(error_code)};
    }

    auto startup = STARTUPINFOW {};
    startup.cb = sizeof(STARTUPINFOW);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = stdout_write.get();
    startup.hStdError = stderr_write.get();
    auto process_info = PROCESS_INFORMATION {};
    auto mutable_command = command_line(request);
    mutable_command.push_back(L'\0');
    const BOOL created = CreateProcessW(nullptr,
        mutable_command.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startup,
        &process_info);
    if (created == FALSE) {
        const DWORD error_code = GetLastError();
        return ProcessResult {.error_message = windows_error_message(error_code)};
    }

    auto process_handle = Handle {process_info.hProcess};
    auto thread_handle = Handle {process_info.hThread};
    stdout_write.reset();
    stderr_write.reset();

    auto result = ProcessResult {};
    auto stdout_thread =
        std::thread {read_pipe, stdout_read.get(), std::ref(result.standard_output), request.stdout_limit_bytes};
    auto stderr_thread =
        std::thread {read_pipe, stderr_read.get(), std::ref(result.standard_error), request.stderr_limit_bytes};

    const auto timeout_count = std::clamp<i64>(request.timeout.count(), 0, std::numeric_limits<DWORD>::max());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds {timeout_count};
    auto wait_result = DWORD {WAIT_TIMEOUT};
    auto cancelled = false;
    while (true) {
        if (request.stop_token.stop_requested()) {
            cancelled = true;
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        const auto remaining =
            now >= deadline ? i64 {0} : std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        constexpr auto cancellation_poll_ms = i64 {50};
        const auto wait_ms = static_cast<DWORD>((std::min)(remaining, cancellation_poll_ms));
        wait_result = WaitForSingleObject(process_handle.get(), wait_ms);
        if (wait_result != WAIT_TIMEOUT || std::chrono::steady_clock::now() >= deadline) {
            break;
        }
    }

    if (cancelled) {
        TerminateProcess(process_handle.get(), 1);
        WaitForSingleObject(process_handle.get(), INFINITE);
        result.state = ProcessExitState::Cancelled;
        result.error_message = "Process cancellation was requested.";
    } else if (wait_result == WAIT_TIMEOUT) {
        TerminateProcess(process_handle.get(), 1);
        WaitForSingleObject(process_handle.get(), INFINITE);
        result.state = ProcessExitState::TimedOut;
        result.error_message = "Process exceeded its timeout.";
    } else if (wait_result != WAIT_OBJECT_0) {
        result.state = ProcessExitState::Crashed;
        result.error_message = windows_error_message(GetLastError());
    } else {
        auto exit_code = DWORD {0};
        if (GetExitCodeProcess(process_handle.get(), &exit_code) == FALSE) {
            result.state = ProcessExitState::Crashed;
            result.error_message = windows_error_message(GetLastError());
        } else {
            if (exit_code == 0) {
                result.state = ProcessExitState::Success;
            } else if (exit_code >= 0xC0000000U) {
                result.state = ProcessExitState::Crashed;
            } else {
                result.state = ProcessExitState::NonzeroExit;
                result.exit_code = static_cast<i32>(exit_code);
            }
        }
    }

    stdout_thread.join();
    stderr_thread.join();
    return result;
#else
    (void)request;
    return ProcessResult {.error_message = "Native process execution is not implemented on this platform."};
#endif
}

} // namespace vidchopper
