#include "cli/cli_settings.hpp"
#include "core/config_paths.hpp"
#include "core/path_utils.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <string>

using namespace vidchopper;

namespace {

auto write_text(const Path& path, const std::string& text) -> void {
    std::filesystem::create_directories(path.parent_path());
    auto stream = std::ofstream {path};
    stream << text;
}

[[nodiscard]] auto file_exists(const Path& path) -> bool {
    return std::filesystem::exists(path);
}

} // namespace

auto main() -> int {
    const auto root = Path {std::filesystem::temp_directory_path() / path_from_utf8("vidchopper CLI settings - 测试")};
    std::filesystem::remove_all(root);

    const auto executable_path = Path {root / "bin" / "VidChopperCLI.exe"};
    const ConfigPathOptions portable_options {.portable = true};
    const CliSettingsPaths paths = resolve_cli_settings_paths(executable_path, false, portable_options);

    test_support::expect_eq(paths.cli_settings_path.filename().string(),
        std::string {"VidChopperCLI.ini"},
        "CLI settings should use VidChopperCLI.ini");
    test_support::expect_eq(paths.gui_settings_path.filename().string(),
        std::string {"VidChopper.ini"},
        "GUI settings path should remain VidChopper.ini");
    test_support::expect_eq(paths.mode, ConfigMode::Portable, "portable settings mode should be explicit");

    const bool cli_file_created = ensure_cli_settings_file(paths.cli_settings_path);
    test_support::expect_true(cli_file_created, "CLI settings file should be created");
    test_support::expect_true(file_exists(paths.cli_settings_path), "VidChopperCLI.ini should exist");
    test_support::expect_true(!file_exists(paths.gui_settings_path), "ensure should not create GUI INI");
    const CliResolvedSettings defaults = load_cli_settings(paths);
    test_support::expect_true(
        !defaults.export_settings.stop_on_first_error, "CLI should continue after chapter failures by default");

    write_text(paths.gui_settings_path, "[encoding]\nx264Crf=40\nnvencCq=41\nffmpegThreads=8\n");
    write_text(paths.cli_settings_path, "x264_crf=20\nstop_on_first_error=false\n");

    const CliResolvedSettings cli_only = load_cli_settings(paths);
    const bool loaded_cli = cli_only.loaded_cli_settings;
    const bool loaded_gui_without_opt_in = cli_only.loaded_gui_settings;
    const bool stop_on_first_error = cli_only.export_settings.stop_on_first_error;
    test_support::expect_true(loaded_cli, "CLI settings should load from VidChopperCLI.ini");
    test_support::expect_true(!loaded_gui_without_opt_in, "GUI settings should require opt-in");
    test_support::expect_true(!stop_on_first_error, "CLI INI should set stop behavior");
    test_support::expect_eq(cli_only.export_settings.x264_crf, u8 {20}, "CLI CRF should come from CLI INI");
    test_support::expect_eq(cli_only.export_settings.nvenc_cq, u8 {22}, "GUI CQ should be ignored");
    test_support::expect_eq(cli_only.export_settings.ffmpeg_threads, u8 {0}, "GUI threads should be ignored");

    const CliSettingsPaths paths_with_gui = resolve_cli_settings_paths(executable_path, true, portable_options);
    const CliResolvedSettings with_gui = load_cli_settings(paths_with_gui);
    test_support::expect_true(with_gui.loaded_gui_settings, "GUI settings should load only with opt-in");
    test_support::expect_eq(with_gui.export_settings.x264_crf, u8 {20}, "CLI INI should override GUI CRF");
    test_support::expect_eq(with_gui.export_settings.nvenc_cq, u8 {41}, "GUI CQ should import");
    test_support::expect_eq(with_gui.export_settings.ffmpeg_threads, u8 {8}, "GUI threads should import");

    auto arguments = CliArguments {};
    arguments.crf = u8 {31};
    arguments.cq = u8 {32};
    arguments.threads = u8 {2};
    arguments.preset = "fast";
    arguments.stop_on_first_error = true;

    const ExportSettings flagged = apply_cli_flag_overrides(cli_only.export_settings, arguments);
    test_support::expect_eq(flagged.x264_crf, u8 {31}, "CLI flag should override loaded CRF");
    test_support::expect_eq(flagged.nvenc_cq, u8 {32}, "CLI flag should override loaded CQ");
    test_support::expect_eq(flagged.ffmpeg_threads, u8 {2}, "CLI flag should override loaded threads");
    test_support::expect_eq(flagged.x264_preset, std::string {"fast"}, "CLI preset should override x264");
    test_support::expect_eq(flagged.nvenc_preset, std::string {"fast"}, "CLI preset should override NVENC");
    test_support::expect_true(flagged.stop_on_first_error, "CLI flag should override stop behavior");

    const ConfigEnvironment windows_environment {
        .platform = ConfigPlatform::Windows,
    };
    const ConfigResolutionResult windows =
        resolve_config_paths(executable_path, ConfigStore::Cli, {}, windows_environment);
    test_support::expect_true(windows.ok(), "Windows config resolution should remain available");
    test_support::expect_eq(windows.paths.settings_path,
        root / "bin" / "VidChopperCLI.ini",
        "Windows native settings should remain adjacent to the executable");

    const Path home = root / "home";
    const ConfigEnvironment mac_environment {
        .platform = ConfigPlatform::MacOS,
        .home_directory = home,
    };
    const ConfigResolutionResult mac = resolve_config_paths(executable_path, ConfigStore::Gui, {}, mac_environment);
    test_support::expect_true(mac.ok(), "macOS config resolution should succeed with HOME");
    test_support::expect_eq(mac.paths.settings_path,
        home / "Library" / "Application Support" / "VidChopper" / "VidChopper.ini",
        "macOS GUI settings should use the native application support root");

    const ConfigEnvironment linux_environment {
        .platform = ConfigPlatform::Linux,
        .home_directory = home,
        .xdg_config_home = root / "xdg",
    };
    const ConfigResolutionResult linux = resolve_config_paths(executable_path, ConfigStore::Cli, {}, linux_environment);
    test_support::expect_true(linux.ok(), "Linux config resolution should succeed with XDG_CONFIG_HOME");
    test_support::expect_eq(linux.paths.settings_path,
        root / "xdg" / "VidChopper" / "VidChopperCLI.ini",
        "Linux CLI settings should use XDG_CONFIG_HOME");

    const ConfigEnvironment linux_home_fallback {
        .platform = ConfigPlatform::Linux,
        .home_directory = home,
    };
    const ConfigResolutionResult linux_fallback =
        resolve_config_paths(executable_path, ConfigStore::Gui, {}, linux_home_fallback);
    test_support::expect_true(linux_fallback.ok(), "Linux config resolution should fall back to HOME");
    test_support::expect_eq(linux_fallback.paths.settings_path,
        home / ".config" / "VidChopper" / "VidChopper.ini",
        "Linux config should use the HOME XDG fallback when no override is set");

    const Path bundled_executable = root / "Applications" / "VidChopper.app" / "Contents" / "MacOS" / "VidChopper";
    const ConfigResolutionResult bundle = resolve_config_paths(
        bundled_executable, ConfigStore::Gui, ConfigPathOptions {.portable = true}, mac_environment);
    test_support::expect_true(bundle.ok(), "macOS portable bundle resolution should succeed");
    test_support::expect_eq(bundle.paths.settings_path,
        root / "Applications" / "VidChopper.ini",
        "macOS portable settings should live beside the outer app bundle");
    test_support::expect_true(path_to_utf8(bundle.paths.settings_path).find("Contents") == std::string::npos,
        "macOS portable settings must not be placed inside Contents");

    const Path explicit_path = root / path_from_utf8("chosen config/设置 🎬.ini");
    const ConfigResolutionResult explicit_result = resolve_config_paths(
        executable_path, ConfigStore::Cli, ConfigPathOptions {.explicit_path = explicit_path}, linux_environment);
    test_support::expect_true(explicit_result.ok(), "explicit config resolution should succeed");
    test_support::expect_eq(
        explicit_result.paths.settings_path, explicit_path, "explicit config path should be authoritative");
    const CliSettingsPaths explicit_cli_paths =
        resolve_cli_settings_paths(executable_path, true, ConfigPathOptions {.explicit_path = explicit_path});
    test_support::expect_eq(explicit_cli_paths.mode, ConfigMode::Explicit, "explicit CLI settings should retain mode");
    test_support::expect_true(
        ensure_cli_settings_file(explicit_cli_paths.cli_settings_path), "explicit CLI settings should be writable");
    write_text(explicit_cli_paths.cli_settings_path, "x264_crf=33\n");
    const CliResolvedSettings explicit_loaded = load_cli_settings(explicit_cli_paths);
    test_support::expect_true(explicit_loaded.loaded_cli_settings, "explicit CLI settings should load");
    test_support::expect_true(
        !explicit_loaded.loaded_gui_settings, "explicit CLI settings should not merge the GUI store");
    test_support::expect_eq(
        explicit_loaded.export_settings.x264_crf, u8 {33}, "explicit CLI settings should retain their values");

    const Path blocked_parent = root / "blocked-parent";
    write_text(blocked_parent, "not a directory");
    const CliSettingsPaths blocked_paths = resolve_cli_settings_paths(
        executable_path, false, ConfigPathOptions {.explicit_path = blocked_parent / "settings.ini"});
    test_support::expect_true(!ensure_cli_settings_file(blocked_paths.cli_settings_path),
        "explicit settings write failures should remain visible to the caller");

    const ConfigEnvironment missing_home {
        .platform = ConfigPlatform::MacOS,
    };
    const ConfigResolutionResult missing_home_result =
        resolve_config_paths(executable_path, ConfigStore::Gui, {}, missing_home);
    test_support::expect_true(!missing_home_result.ok(), "native config resolution should require HOME");

    const ConfigEnvironment relative_xdg {
        .platform = ConfigPlatform::Linux,
        .home_directory = home,
        .xdg_config_home = Path {"relative-config"},
    };
    const ConfigResolutionResult relative_xdg_result =
        resolve_config_paths(executable_path, ConfigStore::Cli, {}, relative_xdg);
    test_support::expect_true(!relative_xdg_result.ok(), "relative XDG_CONFIG_HOME should be rejected");

    const ConfigResolutionResult conflicting = resolve_config_paths(executable_path,
        ConfigStore::Cli,
        ConfigPathOptions {.explicit_path = explicit_path, .portable = true},
        linux_environment);
    test_support::expect_true(!conflicting.ok(), "explicit and portable config modes should be rejected together");

    std::filesystem::remove_all(root);
    return 0;
}
