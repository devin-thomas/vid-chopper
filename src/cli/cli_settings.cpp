#include "cli/cli_settings.h"

#include <fstream>
#include <system_error>

namespace vidchopper {

namespace {

constexpr const char* cli_settings_file_name = "VidChopperCLI.ini";
constexpr const char* gui_settings_file_name = "VidChopper.ini";

[[nodiscard]] auto current_directory() -> Path {
    std::error_code error {};
    const Path path = std::filesystem::current_path(error);
    return error ? Path {"."} : path;
}

[[nodiscard]] auto application_directory_for(const Path& executable_path) -> Path {
    if (executable_path.empty()) {
        return current_directory();
    }

    if (executable_path.has_parent_path()) {
        return executable_path.parent_path().lexically_normal();
    }

    return current_directory();
}

} // namespace

auto resolve_cli_settings_paths(const Path& executable_path, const bool use_gui_config) -> CliSettingsPaths {
    const Path application_directory = application_directory_for(executable_path);
    return CliSettingsPaths {
        .application_directory = application_directory,
        .cli_settings_path = application_directory / cli_settings_file_name,
        .gui_settings_path = application_directory / gui_settings_file_name,
        .use_gui_config = use_gui_config,
    };
}

auto ensure_cli_settings_file(const Path& settings_path) -> bool {
    std::error_code error {};
    const Path parent = settings_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }

    std::ofstream stream {settings_path, std::ios::app};
    return stream.good();
}

} // namespace vidchopper
