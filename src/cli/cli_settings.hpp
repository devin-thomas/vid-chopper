#pragma once

#include "cli/cli_arguments.hpp"
#include "core/models.hpp"
#include "core/types.hpp"

namespace vidchopper {

struct CliSettingsPaths {
    Path application_directory;
    Path cli_settings_path;
    Path gui_settings_path;
    bool use_gui_config {false};

    [[nodiscard]] auto operator==(const CliSettingsPaths&) const -> bool = default;
};

struct CliResolvedSettings {
    ExportSettings export_settings;
    bool loaded_cli_settings {false};
    bool loaded_gui_settings {false};

    [[nodiscard]] auto operator==(const CliResolvedSettings&) const -> bool = default;
};

[[nodiscard]] auto resolve_cli_settings_paths(const Path& executable_path, bool use_gui_config) -> CliSettingsPaths;
[[nodiscard]] auto ensure_cli_settings_file(const Path& settings_path) -> bool;
[[nodiscard]] auto load_cli_settings(const CliSettingsPaths& paths) -> CliResolvedSettings;
[[nodiscard]] auto apply_cli_flag_overrides(ExportSettings settings, const CliArguments& arguments) -> ExportSettings;

} // namespace vidchopper
