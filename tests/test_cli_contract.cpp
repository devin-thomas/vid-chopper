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

auto write_chapter_config(const Path& path,
    const std::string_view folder = "%source%_chapters",
    const std::string_view naming_pattern = "%index% - %name%",
    const u8 crf = 19) -> void {
    std::filesystem::create_directories(path.parent_path());
    auto output = std::ofstream {path};
    if (path.extension() == ".json") {
        output << "{\"version\":1,\"output\":{\"folder\":\"" << folder << "\",\"namingPattern\":\"" << naming_pattern
               << "\"},\"encoder\":{\"crf\":" << static_cast<int>(crf)
               << "},\"chapters\":[{\"name\":\"Match\",\"start\":0,\"end\":2000}]}\n";
        return;
    }
    output << "version: 1\noutput:\n  folder: \"" << folder << "\"\n  namingPattern: \"" << naming_pattern
           << "\"\nencoder:\n  crf: " << static_cast<int>(crf)
           << "\nchapters:\n  - name: Match\n    start: 0\n    end: 2000\n";
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
    ProcessExecutor executor = probe_with_chapters,
    DirectoryScanner directory_scanner = {}) -> CliRunSnapshot {
    auto output = std::ostringstream {};
    auto error_output = std::ostringstream {};
    auto effective_arguments = arguments;
    effective_arguments.emplace_back("--portable");
    const auto request = CliRunRequest {
        .arguments = std::move(effective_arguments),
        .executable_path = executable_path,
        .output = output,
        .error_output = error_output,
        .process_executor = std::move(executor),
        .directory_scanner = std::move(directory_scanner),
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
    write_chapter_config(json_config_path);
    write_chapter_config(yaml_config_path);

    auto dry_run_process_calls = size_t {0};
    const ProcessExecutor dry_run_executor = [&dry_run_process_calls](const ProcessRequest&) -> ProcessResult {
        ++dry_run_process_calls;
        return probe_with_chapters(ProcessRequest {});
    };
    const CliRunSnapshot dry_run =
        run_with({input_path, json_config_path, "--dry-run"}, executable_path, dry_run_executor);
    const Path cli_settings_path = root / "bin" / "VidChopperCLI.ini";
    const Path output_directory = root / "input_chapters";
    test_support::expect_eq(dry_run.exit_code, CliExitCode::Success, "dry-run should validate successfully");
    test_support::expect_eq(dry_run_process_calls, size_t {1}, "dry-run should probe but never invoke ffmpeg");
    test_support::expect_true(contains(dry_run.output, "ChapterSource: "), "dry-run should report chapter provenance");
    test_support::expect_true(contains(dry_run.output, "Command: "), "dry-run should report planned commands");
    test_support::expect_true(contains(dry_run.output, "Planned chapters: 1"), "dry-run should report chapter counts");
    test_support::expect_true(!std::filesystem::exists(cli_settings_path), "dry-run must not create CLI settings");
    test_support::expect_true(!std::filesystem::exists(output_directory), "dry-run must not create output directories");

    const CliRunSnapshot direct = run_with({input_path, json_config_path}, executable_path);
    test_support::expect_eq(direct.exit_code, CliExitCode::Success, "direct two-argument invocation should run");
    test_support::expect_true(contains(direct.output, "Exporting Job 1/1"), "direct invocation should export the plan");
    test_support::expect_true(
        contains(direct.output, "Summary: exported=1"), "direct invocation should summarize export");
    test_support::expect_true(std::filesystem::exists(output_directory / "vidchopper-manifest.json"),
        "successful export should write JSON manifest");
    test_support::expect_true(direct.error_output.empty(), "direct invocation should not print errors");

    const CliRunSnapshot cli_override = run_with({input_path, json_config_path, "--crf", "21"}, executable_path);
    test_support::expect_eq(cli_override.exit_code, CliExitCode::Success, "explicit CLI override should run");
    test_support::expect_true(
        contains(cli_override.output, "Effective CRF: 21"), "explicit CLI flags should override ChapterFile settings");

    const CliRunSnapshot chop = run_with({"chop", input_path, yaml_config_path}, executable_path);
    test_support::expect_eq(chop.exit_code, CliExitCode::Success, "chop subcommand should run");
    test_support::expect_true(contains(chop.output, "Summary: exported=1"), "chop invocation should summarize export");
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

    const Path collision_source_directory = root / "collision-batch" / "videos";
    const Path collision_config_directory = root / "collision-batch" / "configs";
    touch(collision_source_directory / "alpha.mp4");
    touch(collision_source_directory / "beta.mp4");
    write_chapter_config(collision_config_directory / "alpha.json", "exports", "same");
    write_chapter_config(collision_config_directory / "beta.json", "exports", "SAME");
    auto collision_process_calls = size_t {0};
    const ProcessExecutor collision_executor = [&collision_process_calls](const ProcessRequest& request) {
        ++collision_process_calls;
        return probe_with_chapters(request);
    };
    const CliRunSnapshot collision_batch =
        run_with({collision_source_directory.string(), collision_config_directory.string()},
            executable_path,
            collision_executor);
    test_support::expect_eq(
        collision_batch.exit_code, CliExitCode::Error, "colliding output plan should use validation exit 1");
    test_support::expect_true(contains(collision_batch.error_output, "Output collision"),
        "colliding batch should report the planned output path conflict");
    test_support::expect_eq(
        collision_process_calls, size_t {2}, "the complete batch should be probed before output collision validation");

    const Path incomplete_source_directory = root / "incomplete-batch" / "videos";
    const Path incomplete_config_directory = root / "incomplete-batch" / "configs";
    std::filesystem::create_directories(incomplete_source_directory);
    std::filesystem::create_directories(incomplete_config_directory);
    const DirectoryScanner incomplete_scanner = [incomplete_source_directory](
                                                    const Path& directory) -> DirectoryScanResult {
        if (directory == incomplete_source_directory) {
            return DirectoryScanResult {
                .regular_files = {directory / "partial.mp4"},
                .failures = {{.message = "source iterator failed"}},
                .complete = false,
            };
        }
        return DirectoryScanResult {
            .regular_files = {directory / "partial.json"},
            .failures = {{.message = "config iterator failed"}},
            .complete = false,
        };
    };
    auto incomplete_process_calls = size_t {0};
    const ProcessExecutor incomplete_executor = [&incomplete_process_calls](const ProcessRequest& request) {
        ++incomplete_process_calls;
        return probe_with_chapters(request);
    };
    const CliRunSnapshot incomplete_batch =
        run_with({incomplete_source_directory.string(), incomplete_config_directory.string()},
            executable_path,
            incomplete_executor,
            incomplete_scanner);
    test_support::expect_eq(
        incomplete_batch.exit_code, CliExitCode::ValidationError, "incomplete scan should use validation exit 1");
    test_support::expect_eq(incomplete_process_calls, size_t {0}, "incomplete scan must not start external tools");
    test_support::expect_true(!contains(incomplete_batch.error_output, "Missing ChapterFile"),
        "incomplete scan must not report conclusions from partial inventory");

    const CliRunSnapshot invalid = run_with({input_path, json_config_path, yaml_config_path}, executable_path);
    test_support::expect_eq(invalid.exit_code, CliExitCode::Error, "invalid invocation should use exit code 1");
    test_support::expect_true(contains(invalid.error_output, "Too many positional arguments"),
        "invalid invocation should explain the parse failure");
    test_support::expect_true(contains(invalid.error_output, "Usage:"), "invalid invocation should print usage");

    const ProcessExecutor missing_probe_executor = [](const ProcessRequest&) -> ProcessResult {
        return ProcessResult {.state = ProcessExitState::FailedStart, .error_message = "not found"};
    };
    const CliRunSnapshot missing_probe =
        run_with({input_path, json_config_path}, executable_path, missing_probe_executor);
    test_support::expect_eq(
        missing_probe.exit_code, CliExitCode::ToolingError, "missing ffprobe should use tooling exit code 3");
    test_support::expect_true(
        contains(missing_probe.error_output, "failed start"), "missing ffprobe should be distinguished from a timeout");

    auto export_failure_call = size_t {0};
    const ProcessExecutor export_failure_executor = [&export_failure_call](const ProcessRequest& request) {
        ++export_failure_call;
        if (export_failure_call == 1) {
            return probe_with_chapters(request);
        }
        return ProcessResult {
            .state = ProcessExitState::NonzeroExit,
            .exit_code = 9,
            .standard_error = "encoder rejected input",
        };
    };
    const CliRunSnapshot export_failure =
        run_with({input_path, json_config_path}, executable_path, export_failure_executor);
    test_support::expect_eq(
        export_failure.exit_code, CliExitCode::ExportFailure, "nonzero ffmpeg should use export exit code 2");
    test_support::expect_true(
        contains(export_failure.error_output, "exit code 9"), "ffmpeg failure should retain the process exit code");

    auto duration_mismatch_call = size_t {0};
    const ProcessExecutor duration_mismatch_executor = [&duration_mismatch_call](
                                                           const ProcessRequest& request) -> ProcessResult {
        ++duration_mismatch_call;
        if (duration_mismatch_call == 1) {
            return probe_with_chapters(request);
        }
        if (duration_mismatch_call == 2) {
            return ProcessResult {.state = ProcessExitState::Success};
        }
        return ProcessResult {
            .state = ProcessExitState::Success,
            .standard_output =
                R"({"format":{"duration":"4"},"streams":[{"codec_type":"video","avg_frame_rate":"30/1"}],"chapters":[]})",
        };
    };
    const CliRunSnapshot duration_mismatch =
        run_with({input_path, json_config_path}, executable_path, duration_mismatch_executor);
    test_support::expect_eq(
        duration_mismatch.exit_code, CliExitCode::ExportFailure, "duration mismatch should use export exit code 2");
    test_support::expect_true(contains(duration_mismatch.error_output, "expected 2000 ms, observed 4000 ms"),
        "CLI should surface shared duration-verification diagnostics");

    const CliRunSnapshot help = run_with({"--help"}, executable_path);
    test_support::expect_eq(help.exit_code, CliExitCode::Success, "help should succeed");
    test_support::expect_true(
        contains(help.output, "VidChopperCLI.exe <input-video>"), "help should show direct syntax");
    test_support::expect_true(contains(help.output, "VidChopperCLI.exe chop"), "help should show chop syntax");
    test_support::expect_true(help.error_output.empty(), "help should not print errors");

    const CliRunSnapshot version = run_with({"--version"}, executable_path);
    test_support::expect_eq(version.exit_code, CliExitCode::Success, "version should succeed");
    test_support::expect_eq(
        version.output, std::string {"VidChopperCLI " VIDCHOPPER_DISPLAY_VERSION "\n"}, "version output");
    test_support::expect_true(version.error_output.empty(), "version should not print errors");

    std::filesystem::remove_all(root);
    return 0;
}
