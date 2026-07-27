#include "cli/cli_app.hpp"

#include "cli/chapter_source_policy.hpp"
#include "cli/cli_arguments.hpp"
#include "cli/cli_settings.hpp"

#include <ostream>

namespace vidchopper {

auto run_cli(const CliRunRequest& request) -> CliExitCode {
    const CliParseResult parsed = parse_cli_arguments(request.arguments);
    if (!parsed.ok()) {
        request.error_output << parsed.error_message << "\n\n" << cli_usage();
        return CliExitCode::Error;
    }

    const CliArguments& cli_arguments = parsed.arguments;
    if (cli_arguments.command == CliCommand::Help) {
        request.output << cli_usage();
        return CliExitCode::Success;
    }

    const CliSettingsPaths settings_paths =
        resolve_cli_settings_paths(request.executable_path, cli_arguments.use_gui_config);
    if (!ensure_cli_settings_file(settings_paths.cli_settings_path)) {
        request.error_output << "Could not create or open CLI settings file: ";
        request.error_output << settings_paths.cli_settings_path.string() << "\n";
        return CliExitCode::Error;
    }

    const CliResolvedSettings loaded_settings = load_cli_settings(settings_paths);
    const ExportSettings effective_settings = apply_cli_flag_overrides(loaded_settings.export_settings, cli_arguments);

    if (cli_arguments.input_paths.size() == 1 && cli_arguments.config_paths.empty()) {
        if (cli_arguments.use_embedded_chapters) {
            request.output << "Chapter source: embedded chapters (explicit).\n";
        } else {
            request.error_output << "A JSON or YAML chapter config is required before VidChopperCLI can export.\n"
                                 << "If this source contains embedded chapters, rerun exactly:\n"
                                 << embedded_rerun_command(cli_arguments.input_paths.front()) << "\n";
            return CliExitCode::Error;
        }
    }

    if (cli_arguments.input_paths.empty()
        || (cli_arguments.config_paths.empty() && !cli_arguments.use_embedded_chapters)) {
        request.error_output << "Expected an input video and one explicit chapter source.\n\n" << cli_usage();
        return CliExitCode::Error;
    }

    request.output << "VidChopperCLI phase 1 skeleton\n";
    request.output << "Input: " << cli_arguments.input_paths.front().string() << "\n";
    if (cli_arguments.use_embedded_chapters) {
        request.output << "Config: --embedded\n";
    } else {
        request.output << "Config: " << cli_arguments.config_paths.front().string() << "\n";
    }
    request.output << "CLI settings: " << settings_paths.cli_settings_path.string() << "\n";
    request.output << "Settings loaded: CLI=" << (loaded_settings.loaded_cli_settings ? "yes" : "no");
    request.output << ", GUI=" << (loaded_settings.loaded_gui_settings ? "yes" : "no") << "\n";
    request.output << "Effective CRF: " << static_cast<int>(effective_settings.x264_crf) << "\n";

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
