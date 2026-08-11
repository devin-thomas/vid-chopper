#pragma once

#include "core/types.hpp"

#include <chrono>
#include <functional>
#include <stop_token>
#include <string>
#include <vector>

namespace vidchopper {

enum class ProcessExitState : u8 {
    Success = 0,
    FailedStart = 1,
    TimedOut = 2,
    Crashed = 3,
    NonzeroExit = 4,
    Cancelled = 5,
};

struct ProcessRequest {
    Path executable;
    std::vector<std::string> arguments;
    std::chrono::milliseconds timeout {10000};
    size_t stdout_limit_bytes {1024 * 1024};
    size_t stderr_limit_bytes {4096};
    std::stop_token stop_token;
};

struct ProcessResult {
    ProcessExitState state {ProcessExitState::FailedStart};
    i32 exit_code {0};
    std::string standard_output;
    std::string standard_error;
    std::string error_message;

    [[nodiscard]] auto ok() const noexcept -> bool;
    [[nodiscard]] auto operator==(const ProcessResult&) const -> bool = default;
};

using ProcessExecutor = std::function<ProcessResult(const ProcessRequest&)>;

[[nodiscard]] auto run_process(const ProcessRequest& request) -> ProcessResult;
[[nodiscard]] auto process_exit_state_name(ProcessExitState state) -> std::string;

} // namespace vidchopper
