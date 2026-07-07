#include "cli/cli_app.h"

#include "cli/cli_arguments.h"
#include "cli/cli_settings.h"

#include <ostream>

namespace vidchopper {

auto run_cli(const CliRunRequest& request) -> CliExitCode {
    const CliParseResult parsed = parse_cli_arguments(request.arguments);
    if (!parsed.ok()) {
        request.error_output << parsed.error_message << "\n\n" << cli_usage();
        return CliExitCode::MissingConfig;
    }

    const CliArguments& cli_arguments = parsed.arguments;
    if (cli_arguments.command == CliCommand::Help) {
        request.output << cli_usage();
        return CliExitCode::Success;
    }

    const CliSettingsPaths settings_paths = resolve_cli_settings_paths(request.executable_path, cli_arguments.use_gui_config);
    if (!ensure_cli_settings_file(settings_paths.cli_settings_path)) {
        request.error_output << "Could not create or open CLI settings file: ";
        request.error_output << settings_paths.cli_settings_path.string() << "\n";
        return CliExitCode::RuntimeError;
    }

    if (cli_arguments.input_paths.size() == 1 && cli_arguments.config_paths.empty()) {
        request.error_output << "A JSON or YAML chapter config is required before VidChopperCLI can export.\n"
                             << "Embedded chapter probing and hinting will be added in the next CLI phase.\n";
        return CliExitCode::MissingConfig;
    }

    if (cli_arguments.input_paths.empty() || cli_arguments.config_paths.empty()) {
        request.error_output << "Expected an input video and one chapter config.\n\n" << cli_usage();
        return CliExitCode::MissingConfig;
    }

    request.output << "VidChopperCLI phase 1 skeleton\n";
    request.output << "Input: " << cli_arguments.input_paths.front().string() << "\n";
    request.output << "Config: " << cli_arguments.config_paths.front().string() << "\n";
    request.output << "CLI settings: " << settings_paths.cli_settings_path.string() << "\n";

    if (settings_paths.use_gui_config) {
        request.output << "GUI config import requested: " << settings_paths.gui_settings_path.string() << "\n";
    } else {
        request.output << "GUI config import: disabled unless --use-gui-config is passed.\n";
    }

    if (cli_arguments.dry_run) {
        request.output << "Dry run: enabled.\n";
    }

    request.output << "Export execution is not implemented in this skeleton phase.\n";
    return CliExitCode::Success;
}

} // namespace vidchopper
