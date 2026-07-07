#include "cli/cli_arguments.h"
#include "cli/cli_settings.h"
#include "dummy/dummy_cli_data.h"
#include "test_support.h"

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
    const auto direct = parse(test_support::DummyCliData::direct_tokens());
    test_support::expect_true(direct.ok(), "direct invocation should parse");
    test_support::expect_eq(direct.arguments.command, CliCommand::Chop, "direct invocation should imply chop");
    test_support::expect_eq(direct.arguments.input_paths.size(), 1ULL, "direct invocation should capture one input");
    test_support::expect_eq(direct.arguments.config_paths.size(), 1ULL, "direct invocation should capture one config");

    const auto subcommand = parse(test_support::DummyCliData::chop_tokens());
    test_support::expect_true(subcommand.ok(), "chop subcommand should parse");
    test_support::expect_true(subcommand.arguments.dry_run, "dry-run flag should be captured");
    test_support::expect_eq(subcommand.arguments.input_paths.front(),
        test_support::DummyCliData::input_video_path(),
        "subcommand should capture the input path");

    const auto advanced = parse(test_support::DummyCliData::advanced_tokens());
    test_support::expect_true(advanced.ok(), "advanced flags should parse");
    test_support::expect_true(advanced.arguments.crf.has_value(), "crf should be present");
    test_support::expect_eq(*advanced.arguments.crf, u8 {18}, "crf should parse as u8");
    test_support::expect_eq(*advanced.arguments.cq, u8 {22}, "cq should parse as u8");
    test_support::expect_eq(advanced.arguments.preset, std::string {"slow"}, "preset should parse");
    test_support::expect_eq(*advanced.arguments.threads, u8 {4}, "threads should parse as u8");
    test_support::expect_true(advanced.arguments.use_gui_config, "gui config flag should parse");
    test_support::expect_true(advanced.arguments.stop_on_first_error, "stop-on-first-error flag should parse");

    const auto missing_value = parse({"input.mp4", "chapters.json", "--crf"});
    test_support::expect_true(!missing_value.ok(), "missing option value should fail");

    const auto too_many_positionals = parse({"input.mp4", "one.json", "two.json"});
    test_support::expect_true(!too_many_positionals.ok(), "1:N-style positional input should fail in phase one parser");

    const auto paths = resolve_cli_settings_paths(test_support::DummyCliData::executable_path(), true);
    const auto cli_filename_matches = paths.cli_settings_path.filename() == Path {"VidChopperCLI.ini"};
    const auto gui_filename_matches = paths.gui_settings_path.filename() == Path {"VidChopper.ini"};
    test_support::expect_true(cli_filename_matches, "CLI should resolve its own settings filename");
    test_support::expect_true(gui_filename_matches, "GUI settings path should stay separate");
    test_support::expect_true(paths.use_gui_config, "settings path should preserve explicit GUI config request");

    return 0;
}
