#pragma once

#include <filesystem>

namespace vidchopper {

struct CliSettingsPaths {
    std::filesystem::path application_directory;
    std::filesystem::path cli_settings_path;
    std::filesystem::path gui_settings_path;
    bool use_gui_config {false};

    [[nodiscard]] auto operator==(const CliSettingsPaths&) const -> bool = default;
};

[[nodiscard]] auto resolve_cli_settings_paths(
    const std::filesystem::path& executable_path, bool use_gui_config) -> CliSettingsPaths;
[[nodiscard]] auto ensure_cli_settings_file(const std::filesystem::path& settings_path) -> bool;

} // namespace vidchopper
