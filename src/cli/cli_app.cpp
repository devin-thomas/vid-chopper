#include "cli/cli_app.h"

#include "cli/cli_arguments.h"
#include "cli/cli_settings.h"

#include <ostream>

namespace vidchopper {

namespace {

constexpr auto missing_config_exit_code = 2;
constexpr auto runtime_error_exit_code = 1;
constexpr auto success_exit_code = 0;

} // namespace

auto run_cli(const std::vector<std::string>& arguments,
    const std::filesystem::path& executable_path,
    std::ostream& output,
    std::ostream& error_output) -> int {
    const auto parsed = parse_cli_arguments(arguments);
    if (!parsed.ok()) {
        error_output << parsed.error_message << "\n\n" << cli_usage();
        return missing_config_exit_code;
    }

    const auto& cli_arguments = parsed.arguments;
    if (cli_arguments.command == CliCommand::Help) {
        output << cli_usage();
        return success_exit_code;
    }

    const auto settings_paths = resolve_cli_settings_paths(executable_path, cli_arguments.use_gui_config);
    if (!ensure_cli_settings_file(settings_paths.cli_settings_path)) {
        error_output << "Could not create or open CLI settings file: " << settings_paths.cli_settings_path.string()
                     << "\n";
        return runtime_error_exit_code;
    }

    if (cli_arguments.input_paths.size() == 1 && cli_arguments.config_paths.empty()) {
        error_output << "A JSON or YAML chapter config is required before VidChopperCLI can export.\n"
                     << "Embedded chapter probing and hinting will be added in the next CLI phase.\n";
        return missing_config_exit_code;
    }

    if (cli_arguments.input_paths.empty() || cli_arguments.config_paths.empty()) {
        error_output << "Expected an input video and one chapter config.\n\n" << cli_usage();
        return missing_config_exit_code;
    }

    output << "VidChopperCLI phase 1 skeleton\n";
    output << "Input: " << cli_arguments.input_paths.front().string() << "\n";
    output << "Config: " << cli_arguments.config_paths.front().string() << "\n";
    output << "CLI settings: " << settings_paths.cli_settings_path.string() << "\n";

    if (settings_paths.use_gui_config) {
        output << "GUI config import requested: " << settings_paths.gui_settings_path.string() << "\n";
    } else {
        output << "GUI config import: disabled unless --use-gui-config is passed.\n";
    }

    if (cli_arguments.dry_run) {
        output << "Dry run: enabled.\n";
    }

    output << "Export execution is not implemented in this skeleton phase.\n";
    return success_exit_code;
}

} // namespace vidchopper
