#pragma once

#include "core/types.h"

namespace vidchopper {

struct CliSettingsPaths {
    Path application_directory;
    Path cli_settings_path;
    Path gui_settings_path;
    bool use_gui_config {false};

    [[nodiscard]] auto operator==(const CliSettingsPaths&) const -> bool = default;
};

[[nodiscard]] auto resolve_cli_settings_paths(const Path& executable_path, bool use_gui_config) -> CliSettingsPaths;
[[nodiscard]] auto ensure_cli_settings_file(const Path& settings_path) -> bool;

} // namespace vidchopper
