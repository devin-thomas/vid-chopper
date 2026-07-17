#include "cli/cli_settings.hpp"

#include "core/string_utils.hpp"

#include <charconv>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace vidchopper {

namespace {

constexpr const char* cli_settings_file_name = "VidChopperCLI.ini";
constexpr const char* gui_settings_file_name = "VidChopper.ini";

constexpr auto default_cli_settings_contents =
    "# VidChopper CLI settings\n"
    "# GUI settings remain in VidChopper.ini and are never read unless --use-gui-config is passed.\n"
    "x264_crf=18\n"
    "nvenc_cq=22\n"
    "x264_preset=slow\n"
    "nvenc_preset=p5\n"
    "ffmpeg_threads=0\n"
    "stop_on_first_error=true\n";

[[nodiscard]] auto current_directory() -> Path {
    auto error = std::error_code {};
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

[[nodiscard]] auto path_exists(const Path& path) -> bool {
    auto error = std::error_code {};
    const bool exists = std::filesystem::exists(path, error);
    return !error && exists;
}

[[nodiscard]] auto parse_u8_setting(const std::string_view value) -> std::optional<u8> {
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty()) {
        return std::nullopt;
    }

    auto parsed = u32 {0};
    const char* const first = trimmed.data();
    const char* const last = trimmed.data() + trimmed.size();
    const std::from_chars_result result = std::from_chars(first, last, parsed);
    if (result.ec != std::errc {} || result.ptr != last || parsed > 255) {
        return std::nullopt;
    }

    return static_cast<u8>(parsed);
}

[[nodiscard]] auto parse_bool_setting(const std::string_view value) -> std::optional<bool> {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered == "true" || lowered == "1" || lowered == "yes" || lowered == "on") {
        return true;
    }

    if (lowered == "false" || lowered == "0" || lowered == "no" || lowered == "off") {
        return false;
    }

    return std::nullopt;
}

auto apply_setting(ExportSettings& settings, const std::string_view key, const std::string_view value) -> void {
    if (key == "x264_crf") {
        const std::optional<u8> parsed = parse_u8_setting(value);
        if (parsed.has_value() && *parsed <= 51) {
            settings.x264_crf = *parsed;
        }
        return;
    }

    if (key == "nvenc_cq") {
        const std::optional<u8> parsed = parse_u8_setting(value);
        if (parsed.has_value() && *parsed <= 51) {
            settings.nvenc_cq = *parsed;
        }
        return;
    }

    if (key == "x264_preset") {
        settings.x264_preset = trim_copy(value);
        return;
    }

    if (key == "nvenc_preset") {
        settings.nvenc_preset = trim_copy(value);
        return;
    }

    if (key == "preset") {
        const std::string preset = trim_copy(value);
        settings.x264_preset = preset;
        settings.nvenc_preset = preset;
        return;
    }

    if (key == "ffmpeg_threads") {
        const std::optional<u8> parsed = parse_u8_setting(value);
        if (parsed.has_value()) {
            settings.ffmpeg_threads = *parsed;
        }
        return;
    }

    if (key == "stop_on_first_error") {
        const std::optional<bool> parsed = parse_bool_setting(value);
        if (parsed.has_value()) {
            settings.stop_on_first_error = *parsed;
        }
    }
}

[[nodiscard]] auto apply_settings_file(ExportSettings& settings, const Path& path) -> bool {
    auto stream = std::ifstream {path};
    if (!stream.good()) {
        return false;
    }

    auto line = std::string {};
    while (std::getline(stream, line)) {
        const std::string trimmed = trim_copy(line);
        const bool is_comment = trimmed.starts_with('#') || trimmed.starts_with(';') || trimmed.starts_with('[');
        if (trimmed.empty() || is_comment) {
            continue;
        }

        const std::size_t separator = trimmed.find('=');
        if (separator == std::string::npos) {
            continue;
        }

        const std::string key = trim_copy(std::string_view {trimmed}.substr(0, separator));
        const std::string value = trim_copy(std::string_view {trimmed}.substr(separator + 1));
        apply_setting(settings, key, value);
    }

    return true;
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
    auto error = std::error_code {};
    const Path parent = settings_path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            return false;
        }
    }

    if (path_exists(settings_path)) {
        return true;
    }

    auto stream = std::ofstream {settings_path};
    if (!stream.good()) {
        return false;
    }

    stream << default_cli_settings_contents;
    return stream.good();
}

auto load_cli_settings(const CliSettingsPaths& paths) -> CliResolvedSettings {
    auto result = CliResolvedSettings {};

    if (paths.use_gui_config) {
        result.loaded_gui_settings = apply_settings_file(result.export_settings, paths.gui_settings_path);
    }

    result.loaded_cli_settings = apply_settings_file(result.export_settings, paths.cli_settings_path);
    return result;
}

auto apply_cli_flag_overrides(ExportSettings settings, const CliArguments& arguments) -> ExportSettings {
    if (arguments.crf.has_value()) {
        settings.x264_crf = *arguments.crf;
    }

    if (arguments.cq.has_value()) {
        settings.nvenc_cq = *arguments.cq;
    }

    if (arguments.threads.has_value()) {
        settings.ffmpeg_threads = *arguments.threads;
    }

    if (!arguments.preset.empty()) {
        settings.x264_preset = arguments.preset;
        settings.nvenc_preset = arguments.preset;
    }

    if (arguments.stop_on_first_error) {
        settings.stop_on_first_error = true;
    }

    return settings;
}

} // namespace vidchopper
