#pragma once

#include "core/types.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace vidchopper {

enum class CliExitCode : u8 {
    Success = 0,
    RuntimeError = 1,
    MissingConfig = 2,
};

struct CliRunRequest {
    std::vector<std::string> arguments;
    Path executable_path;
    std::ostream& output;
    std::ostream& error_output;
};

[[nodiscard]] auto run_cli(const CliRunRequest& request) -> CliExitCode;

} // namespace vidchopper
