#include "cli/cli_app.h"
#include "test_support.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

using namespace vidchopper;

namespace {

struct CliRunSnapshot {
    CliExitCode exit_code {CliExitCode::RuntimeError};
    std::string output;
    std::string error_output;
};

[[nodiscard]] auto contains(const std::string_view text, const std::string_view needle) -> bool {
    return text.find(needle) != std::string_view::npos;
}

[[nodiscard]] auto run_with(const std::vector<std::string>& arguments, const Path& executable_path) -> CliRunSnapshot {
    auto output = std::ostringstream {};
    auto error_output = std::ostringstream {};
    const auto request = CliRunRequest {
        .arguments = arguments,
        .executable_path = executable_path,
        .output = output,
        .error_output = error_output,
    };

    const CliExitCode exit_code = run_cli(request);
    return CliRunSnapshot {
        .exit_code = exit_code,
        .output = output.str(),
        .error_output = error_output.str(),
    };
}

} // namespace

auto main() -> int {
    const auto root = Path {std::filesystem::temp_directory_path() / "vidchopper-cli-contract"};
    std::filesystem::remove_all(root);

    const auto executable_path = Path {root / "bin" / "VidChopperCLI.exe"};
    const std::string input_path = (root / "input.mp4").string();
    const std::string json_config_path = (root / "chapters.json").string();
    const std::string yaml_config_path = (root / "chapters.yaml").string();

    const CliRunSnapshot direct = run_with({input_path, json_config_path}, executable_path);
    test_support::expect_eq(direct.exit_code, CliExitCode::Success, "direct two-argument invocation should run");
    test_support::expect_true(contains(direct.output, "Input: "), "direct invocation should print input");
    test_support::expect_true(contains(direct.output, "Config: "), "direct invocation should print config");
    test_support::expect_true(direct.error_output.empty(), "direct invocation should not print errors");

    const CliRunSnapshot chop = run_with({"chop", input_path, yaml_config_path}, executable_path);
    test_support::expect_eq(chop.exit_code, CliExitCode::Success, "chop subcommand should run");
    test_support::expect_true(contains(chop.output, yaml_config_path), "chop invocation should print YAML config path");
    test_support::expect_true(chop.error_output.empty(), "chop invocation should not print errors");

    const CliRunSnapshot missing_config = run_with({input_path}, executable_path);
    test_support::expect_eq(missing_config.exit_code, CliExitCode::MissingConfig, "missing config should fail");
    test_support::expect_true(contains(missing_config.error_output, "JSON or YAML chapter config is required"),
        "missing config should explain explicit chapter source requirement");
    test_support::expect_true(!contains(missing_config.output, "VidChopperCLI phase 1 skeleton"),
        "missing config should not enter command execution path");

    const CliRunSnapshot invalid = run_with({input_path, json_config_path, yaml_config_path}, executable_path);
    test_support::expect_eq(invalid.exit_code, CliExitCode::MissingConfig, "invalid invocation should fail");
    test_support::expect_true(contains(invalid.error_output, "Too many positional arguments"),
        "invalid invocation should explain the parse failure");
    test_support::expect_true(contains(invalid.error_output, "Usage:"), "invalid invocation should print usage");

    const CliRunSnapshot help = run_with({"--help"}, executable_path);
    test_support::expect_eq(help.exit_code, CliExitCode::Success, "help should succeed");
    test_support::expect_true(
        contains(help.output, "VidChopperCLI.exe <input-video>"), "help should show direct syntax");
    test_support::expect_true(contains(help.output, "VidChopperCLI.exe chop"), "help should show chop syntax");
    test_support::expect_true(help.error_output.empty(), "help should not print errors");

    std::filesystem::remove_all(root);
    return 0;
}
