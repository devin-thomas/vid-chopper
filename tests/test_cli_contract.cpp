#include "cli/cli_app.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace vidchopper;

namespace {

struct CliRunSnapshot {
    CliExitCode exit_code {CliExitCode::Error};
    std::string output;
    std::string error_output;
};

[[nodiscard]] auto contains(const std::string_view text, const std::string_view needle) -> bool {
    return text.find(needle) != std::string_view::npos;
}

auto touch(const Path& path) -> void {
    std::filesystem::create_directories(path.parent_path());
    auto output = std::ofstream {path};
    output << "fixture\n";
}

[[nodiscard]] auto probe_with_chapters(const ProcessRequest&) -> ProcessResult {
    return ProcessResult {
        .state = ProcessExitState::Success,
        .standard_output =
            R"({"format":{"duration":"2"},"streams":[{"codec_type":"video","avg_frame_rate":"30/1"}],"chapters":[{"start_time":"0","end_time":"2","tags":{"title":"Match"}}]})",
    };
}

[[nodiscard]] auto probe_without_chapters(const ProcessRequest&) -> ProcessResult {
    return ProcessResult {
        .state = ProcessExitState::Success,
        .standard_output =
            R"({"format":{"duration":"2"},"streams":[{"codec_type":"video","avg_frame_rate":"30/1"}],"chapters":[]})",
    };
}

[[nodiscard]] auto run_with(const std::vector<std::string>& arguments,
    const Path& executable_path,
    ProcessExecutor executor = probe_with_chapters) -> CliRunSnapshot {
    auto output = std::ostringstream {};
    auto error_output = std::ostringstream {};
    const auto request = CliRunRequest {
        .arguments = arguments,
        .executable_path = executable_path,
        .output = output,
        .error_output = error_output,
        .process_executor = std::move(executor),
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
    touch(input_path);
    touch(json_config_path);
    touch(yaml_config_path);

    const CliRunSnapshot direct = run_with({input_path, json_config_path}, executable_path);
    test_support::expect_eq(direct.exit_code, CliExitCode::Success, "direct two-argument invocation should run");
    test_support::expect_true(contains(direct.output, "Input: "), "direct invocation should print input");
    test_support::expect_true(contains(direct.output, "Config: "), "direct invocation should print config");
    test_support::expect_true(direct.error_output.empty(), "direct invocation should not print errors");

    const CliRunSnapshot chop = run_with({"chop", input_path, yaml_config_path}, executable_path);
    test_support::expect_eq(chop.exit_code, CliExitCode::Success, "chop subcommand should run");
    test_support::expect_true(contains(chop.output, yaml_config_path), "chop invocation should print YAML config path");
    test_support::expect_true(chop.error_output.empty(), "chop invocation should not print errors");

    const CliRunSnapshot embedded = run_with({input_path, "--embedded"}, executable_path);
    test_support::expect_eq(embedded.exit_code, CliExitCode::Success, "explicit embedded invocation should run");
    test_support::expect_true(contains(embedded.output, "Chapter source: embedded chapters (explicit)."),
        "embedded invocation should report its explicit source policy");
    test_support::expect_true(
        contains(embedded.output, "Config: --embedded"), "embedded invocation should not report a chapter file");

    const CliRunSnapshot embedded_missing =
        run_with({input_path, "--embedded"}, executable_path, probe_without_chapters);
    test_support::expect_eq(
        embedded_missing.exit_code, CliExitCode::Error, "embedded mode should fail when chapters are absent");
    test_support::expect_true(contains(embedded_missing.error_output, "JSON or YAML chapter config is required"),
        "embedded mode should guide sources without embedded chapters");

    const CliRunSnapshot missing_config = run_with({input_path}, executable_path);
    test_support::expect_eq(missing_config.exit_code, CliExitCode::Error, "missing config should use exit code 1");
    test_support::expect_true(contains(missing_config.error_output, "Embedded chapters were found"),
        "missing config should report discovered embedded chapters without selecting them");
    test_support::expect_true(!contains(missing_config.output, "VidChopperCLI phase 1 skeleton"),
        "missing config should not enter command execution path");
    test_support::expect_true(
        contains(missing_config.error_output, "VidChopperCLI.exe \"" + input_path + "\" --embedded"),
        "missing config should preserve the source path in a safe exact rerun command");

    const CliRunSnapshot missing_without_embedded = run_with({input_path}, executable_path, probe_without_chapters);
    test_support::expect_true(
        contains(missing_without_embedded.error_output, "JSON or YAML chapter config is required"),
        "missing config should require a chapter file when the probe finds no embedded chapters");
    test_support::expect_true(!contains(missing_without_embedded.error_output, "--embedded"),
        "missing config should not recommend embedded mode when the probe finds no chapters");

    const std::string spaced_input_path = (root / "input clips" / "match footage.mkv").string();
    touch(spaced_input_path);
    const CliRunSnapshot spaced_missing_config = run_with({spaced_input_path}, executable_path);
    test_support::expect_true(
        contains(spaced_missing_config.error_output, "VidChopperCLI.exe \"" + spaced_input_path + "\" --embedded"),
        "embedded rerun guidance should quote source paths containing spaces");

    const Path invalid_source_directory = root / "invalid-batch" / "videos";
    const Path invalid_config_directory = root / "invalid-batch" / "configs";
    touch(invalid_source_directory / "alpha.mp4");
    touch(invalid_source_directory / "beta.mkv");
    touch(invalid_config_directory / "alpha.json");
    touch(invalid_config_directory / "gamma.yaml");
    auto process_calls = size_t {0};
    const ProcessExecutor counting_executor = [&process_calls](const ProcessRequest& request) -> ProcessResult {
        ++process_calls;
        return probe_with_chapters(request);
    };
    const CliRunSnapshot invalid_batch = run_with(
        {invalid_source_directory.string(), invalid_config_directory.string()}, executable_path, counting_executor);
    test_support::expect_eq(
        invalid_batch.exit_code, CliExitCode::Error, "invalid batch planning should use exit code 1");
    test_support::expect_true(contains(invalid_batch.error_output, "Missing ChapterFile"),
        "invalid batch planning should print every missing config");
    test_support::expect_true(contains(invalid_batch.error_output, "Orphan ChapterFile"),
        "invalid batch planning should print every orphan config");
    test_support::expect_eq(process_calls, size_t {0}, "invalid batch planning should not start a process");
    test_support::expect_true(
        invalid_batch.output.empty(), "invalid batch planning should not enter the command execution path");

    const CliRunSnapshot invalid = run_with({input_path, json_config_path, yaml_config_path}, executable_path);
    test_support::expect_eq(invalid.exit_code, CliExitCode::Error, "invalid invocation should use exit code 1");
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
