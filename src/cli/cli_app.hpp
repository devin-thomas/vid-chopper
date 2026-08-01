#pragma once

#include "cli/batch_resolver.hpp"
#include "core/types.hpp"
#include "services/probe_service.hpp"

#include <iosfwd>
#include <string>
#include <vector>

namespace vidchopper {

enum class CliExitCode : u8 {
    Success = 0,
    ValidationError = 1,
    Error = ValidationError,
    ExportFailure = 2,
    ToolingError = 3,
};

struct CliRunRequest {
    std::vector<std::string> arguments;
    Path executable_path;
    std::ostream& output;
    std::ostream& error_output;
    ProcessExecutor process_executor {run_process};
    DirectoryScanner directory_scanner;
};

[[nodiscard]] auto run_cli(const CliRunRequest& request) -> CliExitCode;

} // namespace vidchopper
