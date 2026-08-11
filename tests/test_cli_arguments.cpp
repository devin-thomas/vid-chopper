#include "cli/cli_arguments.hpp"
#include "cli/cli_settings.hpp"
#include "core/path_utils.hpp"
#include "dummy/dummy_cli_data.hpp"
#include "test_support.hpp"

#include <initializer_list>
#include <string>
#include <vector>

using namespace vidchopper;

namespace {

[[nodiscard]] auto parse(std::initializer_list<std::string> tokens) -> CliParseResult {
    return parse_cli_arguments(std::vector<std::string> {tokens});
}

[[nodiscard]] auto parse(const std::vector<std::string>& tokens) -> CliParseResult {
    return parse_cli_arguments(tokens);
}

} // namespace

auto main() -> int {
    const CliParseResult direct = parse(test_support::DummyCliData::direct_tokens());
    test_support::expect_true(direct.ok(), "direct invocation should parse");
    test_support::expect_eq(direct.arguments.command, CliCommand::Chop, "direct invocation should imply chop");
    test_support::expect_eq(direct.arguments.input_paths.size(), 1ULL, "direct invocation should capture one input");
    test_support::expect_eq(direct.arguments.config_paths.size(), 1ULL, "direct invocation should capture one config");

    const CliParseResult subcommand = parse(test_support::DummyCliData::chop_tokens());
    test_support::expect_true(subcommand.ok(), "chop subcommand should parse");
    test_support::expect_true(subcommand.arguments.dry_run, "dry-run flag should be captured");
    test_support::expect_eq(subcommand.arguments.input_paths.front(),
        test_support::DummyCliData::input_video_path(),
        "subcommand should capture the input path");

    const CliParseResult version = parse({"--version"});
    test_support::expect_true(version.ok(), "version flag should parse");
    test_support::expect_eq(version.arguments.command, CliCommand::Version, "version flag should select version");

    const CliParseResult advanced = parse(test_support::DummyCliData::advanced_tokens());
    test_support::expect_true(advanced.ok(), "advanced flags should parse");
    test_support::expect_true(advanced.arguments.crf.has_value(), "crf should be present");
    test_support::expect_eq(*advanced.arguments.crf, u8 {18}, "crf should parse as u8");
    test_support::expect_eq(*advanced.arguments.cq, u8 {22}, "cq should parse as u8");
    test_support::expect_eq(advanced.arguments.preset, std::string {"slow"}, "preset should parse");
    test_support::expect_eq(*advanced.arguments.threads, u8 {4}, "threads should parse as u8");
    test_support::expect_true(advanced.arguments.use_gui_config, "gui config flag should parse");
    test_support::expect_true(advanced.arguments.stop_on_first_error, "stop-on-first-error flag should parse");

    const CliParseResult manifests =
        parse({"input.mp4", "chapters.json", "--aggregate-json", "run.json", "--aggregate-csv", "run.csv"});
    test_support::expect_true(manifests.ok(), "aggregate manifest flags should parse");
    test_support::expect_eq(
        *manifests.arguments.aggregate_json_path, Path {"run.json"}, "aggregate JSON path should parse");
    test_support::expect_eq(
        *manifests.arguments.aggregate_csv_path, Path {"run.csv"}, "aggregate CSV path should parse");

    const CliParseResult embedded = parse({"input.mkv", "--embedded"});
    test_support::expect_true(embedded.ok(), "explicit embedded chapter source should parse");
    test_support::expect_true(embedded.arguments.use_embedded_chapters, "embedded source should be captured");
    test_support::expect_true(
        embedded.arguments.config_paths.empty(), "embedded source should not create a config path");

    const CliParseResult conflicting_sources = parse({"input.mkv", "chapters.json", "--embedded"});
    test_support::expect_true(!conflicting_sources.ok(), "config and embedded chapter sources should conflict");

    const CliParseResult missing_value = parse({"input.mp4", "chapters.json", "--crf"});
    test_support::expect_true(!missing_value.ok(), "missing option value should fail");

    const CliParseResult too_many_positionals = parse({"input.mp4", "one.json", "two.json"});
    test_support::expect_true(!too_many_positionals.ok(), "1:N-style positional input should fail in phase one parser");

    const CliSettingsPaths paths = resolve_cli_settings_paths(test_support::DummyCliData::executable_path(), true);
    const bool cli_filename_matches = paths.cli_settings_path.filename() == Path {"VidChopperCLI.ini"};
    const bool gui_filename_matches = paths.gui_settings_path.filename() == Path {"VidChopper.ini"};
    test_support::expect_true(cli_filename_matches, "CLI should resolve its own settings filename");
    test_support::expect_true(gui_filename_matches, "GUI settings path should stay separate");
    test_support::expect_true(paths.use_gui_config, "settings path should preserve explicit GUI config request");

    const CliParseResult config_options =
        parse({"input.mp4", "chapters.json", "--config", "settings/cli.ini", "--portable"});
    test_support::expect_true(config_options.ok(), "explicit config and portable flags should parse");
    test_support::expect_eq(*config_options.arguments.settings_path,
        Path {"settings/cli.ini"},
        "explicit config path should be captured separately from the chapter config");
    test_support::expect_true(config_options.arguments.portable_config, "portable config mode should be captured");

    const std::string unicode_input = "/tmp/VidChopper/媒体 clips/视频 🎬.mp4";
    const std::string unicode_config = "/tmp/VidChopper/章节/第一.json";
    const CliParseResult unicode_paths = parse({unicode_input, unicode_config});
    test_support::expect_true(unicode_paths.ok(), "UTF-8 positional paths should parse");
    test_support::expect_eq(path_to_utf8(unicode_paths.arguments.input_paths.front()),
        unicode_input,
        "CLI input paths should cross the UTF-8 filesystem boundary once");
    test_support::expect_eq(path_to_utf8(unicode_paths.arguments.config_paths.front()),
        unicode_config,
        "CLI chapter paths should preserve UTF-8 text");

    return 0;
}
