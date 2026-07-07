#include "cli/cli_settings.h"
#include "test_support.h"

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
    const auto root = Path {std::filesystem::temp_directory_path() / "vidchopper-cli-settings"};
    std::filesystem::remove_all(root);

    const auto executable_path = Path {root / "bin" / "VidChopperCLI.exe"};
    const CliSettingsPaths paths = resolve_cli_settings_paths(executable_path, false);

    test_support::expect_eq(paths.cli_settings_path.filename().string(),
        std::string {"VidChopperCLI.ini"},
        "CLI settings should use VidChopperCLI.ini");
    test_support::expect_eq(paths.gui_settings_path.filename().string(),
        std::string {"VidChopper.ini"},
        "GUI settings path should remain VidChopper.ini");

    test_support::expect_true(ensure_cli_settings_file(paths.cli_settings_path), "CLI settings file should be created");
    test_support::expect_true(file_exists(paths.cli_settings_path), "VidChopperCLI.ini should exist after ensure");
    test_support::expect_true(!file_exists(paths.gui_settings_path), "ensure should not create VidChopper.ini");

    write_text(paths.gui_settings_path, "x264_crf=40\nnvenc_cq=41\nffmpeg_threads=8\n");
    write_text(paths.cli_settings_path, "x264_crf=20\nstop_on_first_error=false\n");

    const CliResolvedSettings cli_only = load_cli_settings(paths);
    test_support::expect_true(cli_only.loaded_cli_settings, "CLI settings should load from VidChopperCLI.ini");
    test_support::expect_true(!cli_only.loaded_gui_settings, "GUI settings should not load without --use-gui-config");
    test_support::expect_eq(cli_only.export_settings.x264_crf, u8 {20}, "CLI CRF should come from CLI INI");
    test_support::expect_eq(cli_only.export_settings.nvenc_cq, u8 {22}, "GUI CQ should be ignored without opt-in");
    test_support::expect_eq(cli_only.export_settings.ffmpeg_threads, u8 {0}, "GUI threads should be ignored without opt-in");
    test_support::expect_true(!cli_only.export_settings.stop_on_first_error, "CLI INI should set stop behavior");

    const CliSettingsPaths paths_with_gui = resolve_cli_settings_paths(executable_path, true);
    const CliResolvedSettings with_gui = load_cli_settings(paths_with_gui);
    test_support::expect_true(with_gui.loaded_gui_settings, "GUI settings should load only with --use-gui-config");
    test_support::expect_eq(with_gui.export_settings.x264_crf, u8 {20}, "CLI INI should override imported GUI CRF");
    test_support::expect_eq(with_gui.export_settings.nvenc_cq, u8 {41}, "GUI CQ should import when CLI is silent");
    test_support::expect_eq(with_gui.export_settings.ffmpeg_threads, u8 {8}, "GUI threads should import when CLI is silent");

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
    test_support::expect_eq(flagged.x264_preset, std::string {"fast"}, "CLI preset should override x264 preset");
    test_support::expect_eq(flagged.nvenc_preset, std::string {"fast"}, "CLI preset should override NVENC preset");
    test_support::expect_true(flagged.stop_on_first_error, "CLI flag should override stop behavior");

    std::filesystem::remove_all(root);
    return 0;
}
