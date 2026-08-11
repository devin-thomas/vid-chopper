#include "services/process_runner.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

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

auto read_pipe(const HANDLE pipe,
    std::string& output,
    const size_t limit,
    const std::function<void(std::string_view)>& chunk_callback) -> void {
    auto buffer = std::array<char, 4096> {};
    auto bytes_read = DWORD {0};
    while (ReadFile(pipe, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr) != FALSE) {
        const size_t remaining = output.size() < limit ? limit - output.size() : 0;
        const size_t count = (std::min)(remaining, static_cast<size_t>(bytes_read));
        output.append(buffer.data(), count);
        if (count > 0 && chunk_callback) {
            chunk_callback(std::string_view {buffer.data(), count});
        }
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

#ifndef _WIN32

class FileDescriptor final {
public:
    FileDescriptor() = default;
    explicit FileDescriptor(const int value)
        : value_ {value} {
    }
    FileDescriptor(const FileDescriptor&) = delete;
    auto operator=(const FileDescriptor&) -> FileDescriptor& = delete;
    FileDescriptor(FileDescriptor&& other) noexcept
        : value_ {other.release()} {
    }
    auto operator=(FileDescriptor&& other) noexcept -> FileDescriptor& {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }
    ~FileDescriptor() {
        reset();
    }

    [[nodiscard]] auto get() const noexcept -> int {
        return value_;
    }

    [[nodiscard]] auto valid() const noexcept -> bool {
        return value_ >= 0;
    }

    auto reset(const int value = -1) noexcept -> void {
        if (valid()) {
            static_cast<void>(::close(value_));
        }
        value_ = value;
    }

    [[nodiscard]] auto release() noexcept -> int {
        const int value = value_;
        value_ = -1;
        return value;
    }

private:
    int value_ {-1};
};

enum class ChildStartOperation : i32 {
    ProcessGroup = 1,
    StandardOutput = 2,
    StandardError = 3,
    CloseOnExec = 4,
    Execute = 5,
};

struct ChildStartError {
    ChildStartOperation operation {ChildStartOperation::Execute};
    i32 error_number {0};
};

inline constexpr auto process_termination_grace = std::chrono::milliseconds {250};

[[nodiscard]] auto posix_error_message(const std::string_view operation, const int error_number) -> std::string {
    return std::format("{}: {}", operation, std::strerror(error_number));
}

[[nodiscard]] auto set_close_on_exec(const int descriptor) -> bool {
    const int flags = ::fcntl(descriptor, F_GETFD, 0);
    return flags >= 0 && ::fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC) == 0;
}

[[nodiscard]] auto set_nonblocking(const int descriptor) -> bool {
    const int flags = ::fcntl(descriptor, F_GETFL, 0);
    return flags >= 0 && ::fcntl(descriptor, F_SETFL, flags | O_NONBLOCK) == 0;
}

[[nodiscard]] auto create_pipe(FileDescriptor& read_end, FileDescriptor& write_end) -> bool {
    auto descriptors = std::array<int, 2> {};
    if (::pipe(descriptors.data()) != 0) {
        return false;
    }
    read_end.reset(descriptors[0]);
    write_end.reset(descriptors[1]);
    return true;
}

auto report_child_start_error(
    const int descriptor, const ChildStartOperation operation, const int error_number) noexcept -> void {
    const auto error = ChildStartError {.operation = operation, .error_number = error_number};
    const auto* bytes = reinterpret_cast<const char*>(&error);
    size_t written = size_t {0};
    while (written < sizeof(error)) {
        const ssize_t count = ::write(descriptor, bytes + written, sizeof(error) - written);
        if (count > 0) {
            written += static_cast<size_t>(count);
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    _exit(127);
}

auto drain_pipe(FileDescriptor& descriptor,
    std::string& output,
    const size_t limit,
    const std::function<void(std::string_view)>& chunk_callback,
    std::string& pipe_error) -> void {
    auto buffer = std::array<char, 8192> {};
    while (descriptor.valid()) {
        const ssize_t bytes_read = ::read(descriptor.get(), buffer.data(), buffer.size());
        if (bytes_read > 0) {
            const size_t available = output.size() < limit ? limit - output.size() : 0;
            const size_t count = (std::min)(available, static_cast<size_t>(bytes_read));
            if (count > 0) {
                output.append(buffer.data(), count);
                if (chunk_callback) {
                    chunk_callback(std::string_view {buffer.data(), count});
                }
            }
            continue;
        }
        if (bytes_read == 0) {
            descriptor.reset();
            return;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return;
        }
        if (pipe_error.empty()) {
            pipe_error = posix_error_message("Could not read child output", errno);
        }
        descriptor.reset();
    }
}

[[nodiscard]] auto child_start_operation_name(const ChildStartOperation operation) -> std::string_view {
    switch (operation) {
    case ChildStartOperation::ProcessGroup:
        return "create process group";
    case ChildStartOperation::StandardOutput:
        return "redirect standard output";
    case ChildStartOperation::StandardError:
        return "redirect standard error";
    case ChildStartOperation::CloseOnExec:
        return "configure startup pipe";
    case ChildStartOperation::Execute:
        return "execute process";
    }
    return "start process";
}

#endif

} // namespace

auto ProcessResult::ok() const noexcept -> bool {
    return state == ProcessExitState::Success;
}

auto is_default_process_executor(const ProcessExecutor& executor) noexcept -> bool {
    const auto target = executor.target<ProcessResult (*)(const ProcessRequest&)>();
    return target != nullptr && *target == &run_process;
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
        return ProcessResult {
            .state = ProcessExitState::FailedStart,
            .error_message = std::format("execute process: {}", windows_error_message(error_code)),
        };
    }

    auto process_handle = Handle {process_info.hProcess};
    auto thread_handle = Handle {process_info.hThread};
    stdout_write.reset();
    stderr_write.reset();

    auto result = ProcessResult {};
    auto stdout_thread = std::thread {read_pipe,
        stdout_read.get(),
        std::ref(result.standard_output),
        request.stdout_limit_bytes,
        std::cref(request.standard_output_chunk)};
    auto stderr_thread = std::thread {read_pipe,
        stderr_read.get(),
        std::ref(result.standard_error),
        request.stderr_limit_bytes,
        std::cref(request.standard_error_chunk)};

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
    auto stdout_read = FileDescriptor {};
    auto stdout_write = FileDescriptor {};
    auto stderr_read = FileDescriptor {};
    auto stderr_write = FileDescriptor {};
    auto startup_error_read = FileDescriptor {};
    auto startup_error_write = FileDescriptor {};
    if (!create_pipe(stdout_read, stdout_write) || !create_pipe(stderr_read, stderr_write)
        || !create_pipe(startup_error_read, startup_error_write)) {
        return ProcessResult {
            .state = ProcessExitState::FailedStart,
            .error_message = posix_error_message("Could not create process pipes", errno),
        };
    }
    if (!set_close_on_exec(startup_error_write.get())) {
        return ProcessResult {
            .state = ProcessExitState::FailedStart,
            .error_message = posix_error_message("Could not configure startup pipe", errno),
        };
    }
    if (!set_nonblocking(stdout_read.get()) || !set_nonblocking(stderr_read.get())) {
        return ProcessResult {
            .state = ProcessExitState::FailedStart,
            .error_message = posix_error_message("Could not configure output pipes", errno),
        };
    }

    auto executable_text = request.executable.string();
    auto argument_storage = request.arguments;
    auto argv = std::vector<char*> {};
    argv.reserve(argument_storage.size() + 2);
    argv.push_back(executable_text.data());
    for (std::string& argument : argument_storage) {
        argv.push_back(argument.data());
    }
    argv.push_back(nullptr);

    const pid_t child = ::fork();
    if (child < 0) {
        return ProcessResult {
            .state = ProcessExitState::FailedStart,
            .error_message = posix_error_message("Could not fork process", errno),
        };
    }
    if (child == 0) {
        if (::setpgid(0, 0) != 0) {
            report_child_start_error(startup_error_write.get(), ChildStartOperation::ProcessGroup, errno);
        }
        if (::dup2(stdout_write.get(), STDOUT_FILENO) < 0) {
            report_child_start_error(startup_error_write.get(), ChildStartOperation::StandardOutput, errno);
        }
        if (::fcntl(STDOUT_FILENO, F_SETFD, 0) != 0) {
            report_child_start_error(startup_error_write.get(), ChildStartOperation::CloseOnExec, errno);
        }
        if (::dup2(stderr_write.get(), STDERR_FILENO) < 0) {
            report_child_start_error(startup_error_write.get(), ChildStartOperation::StandardError, errno);
        }
        if (::fcntl(STDERR_FILENO, F_SETFD, 0) != 0) {
            report_child_start_error(startup_error_write.get(), ChildStartOperation::CloseOnExec, errno);
        }

        const auto descriptors = std::array<int, 5> {
            stdout_read.get(), stdout_write.get(), stderr_read.get(), stderr_write.get(), startup_error_read.get()};
        for (const int descriptor : descriptors) {
            if (descriptor >= 0 && descriptor != STDOUT_FILENO && descriptor != STDERR_FILENO
                && descriptor != startup_error_write.get()) {
                static_cast<void>(::close(descriptor));
            }
        }
        ::execvp(argv.front(), argv.data());
        report_child_start_error(startup_error_write.get(), ChildStartOperation::Execute, errno);
    }

    stdout_write.reset();
    stderr_write.reset();
    startup_error_write.reset();
    auto process_group_error = std::string {};
    if (::setpgid(child, child) != 0 && errno != EACCES && errno != ESRCH) {
        process_group_error = posix_error_message("Could not confirm process group", errno);
    }

    auto result = ProcessResult {};
    auto wait_status = 0;
    bool child_reaped = false;
    auto wait_error = std::string {};
    auto pipe_error = std::string {};
    auto signal_error = std::string {};
    auto termination_state = std::optional<ProcessExitState> {};
    auto termination_deadline = std::chrono::steady_clock::time_point {};
    bool kill_sent = false;
    const auto timeout_count = (std::max)(i64 {0}, static_cast<i64>(request.timeout.count()));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds {timeout_count};

    const auto reap_child = [&]() -> void {
        while (!child_reaped && wait_error.empty()) {
            const pid_t waited = ::waitpid(child, &wait_status, WNOHANG);
            if (waited == child) {
                child_reaped = true;
            } else if (waited == 0) {
                return;
            } else if (waited < 0 && errno == EINTR) {
                continue;
            } else if (waited < 0) {
                wait_error = posix_error_message("Could not reap process", errno);
            }
        }
    };

    const auto signal_group = [&](const int signal) -> void {
        if (::kill(-child, signal) != 0 && errno != ESRCH && signal_error.empty()) {
            signal_error = posix_error_message("Could not signal process group", errno);
        }
    };

    const auto begin_termination = [&](const ProcessExitState state) -> void {
        termination_state = state;
        termination_deadline = std::chrono::steady_clock::now() + process_termination_grace;
        signal_group(SIGTERM);
    };

    const auto poll_output = [&](const int timeout_ms) -> void {
        auto descriptors = std::array<pollfd, 2> {};
        nfds_t count = 0;
        if (stdout_read.valid()) {
            descriptors[count++] = pollfd {.fd = stdout_read.get(), .events = POLLIN};
        }
        if (stderr_read.valid()) {
            descriptors[count++] = pollfd {.fd = stderr_read.get(), .events = POLLIN};
        }
        while (true) {
            const int polled = ::poll(descriptors.data(), count, timeout_ms);
            if (polled >= 0 || errno != EINTR) {
                break;
            }
        }
        drain_pipe(
            stdout_read, result.standard_output, request.stdout_limit_bytes, request.standard_output_chunk, pipe_error);
        drain_pipe(
            stderr_read, result.standard_error, request.stderr_limit_bytes, request.standard_error_chunk, pipe_error);
    };

    const auto finish_group_termination = [&]() -> void {
        if (!child_reaped || !termination_state.has_value() || kill_sent) {
            return;
        }
        while (std::chrono::steady_clock::now() < termination_deadline) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                termination_deadline - std::chrono::steady_clock::now())
                                       .count();
            poll_output(static_cast<int>((std::min)(i64 {50}, (std::max)(i64 {0}, remaining))));
        }
        signal_group(SIGKILL);
        kill_sent = true;
    };

    while (!child_reaped && wait_error.empty()) {
        reap_child();
        if (child_reaped || !wait_error.empty()) {
            break;
        }

        const auto now = std::chrono::steady_clock::now();
        if (!termination_state.has_value()) {
            if (request.stop_token.stop_requested()) {
                begin_termination(ProcessExitState::Cancelled);
            } else if (now >= deadline) {
                begin_termination(ProcessExitState::TimedOut);
            }
        } else if (!kill_sent && now >= termination_deadline) {
            signal_group(SIGKILL);
            kill_sent = true;
        }

        auto wait_ms = i64 {50};
        if (!termination_state.has_value()) {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
            wait_ms = (std::min)(wait_ms, (std::max)(i64 {0}, remaining));
        } else if (!kill_sent) {
            const auto remaining =
                std::chrono::duration_cast<std::chrono::milliseconds>(termination_deadline - now).count();
            wait_ms = (std::min)(wait_ms, (std::max)(i64 {0}, remaining));
        }
        poll_output(static_cast<int>((std::min)(wait_ms, i64 {50})));
    }
    finish_group_termination();

    while (!child_reaped && wait_error.empty()) {
        reap_child();
        if (child_reaped || !wait_error.empty()) {
            break;
        }
        poll_output(50);
    }
    finish_group_termination();

    while (stdout_read.valid() || stderr_read.valid()) {
        drain_pipe(
            stdout_read, result.standard_output, request.stdout_limit_bytes, request.standard_output_chunk, pipe_error);
        drain_pipe(
            stderr_read, result.standard_error, request.stderr_limit_bytes, request.standard_error_chunk, pipe_error);
        if (!stdout_read.valid() && !stderr_read.valid()) {
            break;
        }
        poll_output(50);
    }

    auto startup_error = ChildStartError {};
    auto startup_error_bytes = size_t {0};
    while (startup_error_bytes < sizeof(startup_error)) {
        const ssize_t bytes_read = ::read(startup_error_read.get(),
            reinterpret_cast<char*>(&startup_error) + startup_error_bytes,
            sizeof(startup_error) - startup_error_bytes);
        if (bytes_read > 0) {
            startup_error_bytes += static_cast<size_t>(bytes_read);
        } else if (bytes_read < 0 && errno == EINTR) {
            continue;
        } else {
            break;
        }
    }
    startup_error_read.reset();

    if (!wait_error.empty()) {
        result.state = ProcessExitState::Crashed;
        result.error_message = wait_error;
    } else if (startup_error_bytes == sizeof(startup_error) && !termination_state.has_value()) {
        result.state = ProcessExitState::FailedStart;
        result.error_message = std::format(
            "{}: {}", child_start_operation_name(startup_error.operation), std::strerror(startup_error.error_number));
    } else if (termination_state.has_value()) {
        result.state = *termination_state;
        result.error_message = *termination_state == ProcessExitState::Cancelled ? "Process cancellation was requested."
                                                                                 : "Process exceeded its timeout.";
        if (WIFSIGNALED(wait_status)) {
            result.termination_signal = WTERMSIG(wait_status);
        }
    } else if (WIFEXITED(wait_status)) {
        result.exit_code = WEXITSTATUS(wait_status);
        result.state = result.exit_code == 0 ? ProcessExitState::Success : ProcessExitState::NonzeroExit;
    } else if (WIFSIGNALED(wait_status)) {
        result.state = ProcessExitState::Crashed;
        result.termination_signal = WTERMSIG(wait_status);
        result.error_message = std::format("Process terminated by signal {}.", result.termination_signal);
    } else {
        result.state = ProcessExitState::Crashed;
        result.error_message = "Process ended without an exit status.";
    }

    if (!process_group_error.empty()) {
        result.error_message += (result.error_message.empty() ? "" : " ") + process_group_error;
    }
    if (!signal_error.empty()) {
        result.error_message += (result.error_message.empty() ? "" : " ") + signal_error;
    }
    if (!pipe_error.empty()) {
        result.error_message += (result.error_message.empty() ? "" : " ") + pipe_error;
    }
    return result;
#endif
}

} // namespace vidchopper
