#include "core/chapter_plan.hpp"
#include "core/command_builder.hpp"
#include "test_support.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <process.h>
#include <sstream>
#include <string>

using namespace vidchopper;

namespace {

auto quote(const std::string& value) -> std::string {
    auto quoted = std::string {"\""};
    for (const auto character : value) {
        if (character == '"') {
            quoted.push_back('\\');
        }

        quoted.push_back(character);
    }

    quoted.push_back('"');
    return quoted;
}

auto run_capture(const std::string& command) -> std::string {
    const auto handle = _popen(command.c_str(), "r");
    test_support::expect_true(handle != nullptr, "command should be spawnable");

    auto output = std::string {};
    auto buffer = std::array<char, 512> {};
    while (std::fgets(buffer.data(), static_cast<int>(buffer.size()), handle) != nullptr) {
        output += buffer.data();
    }

    const auto exit_code = _pclose(handle);
    test_support::expect_eq(exit_code, 0, "captured command should succeed");
    return output;
}

auto run_args_no_capture(const std::vector<std::string>& command) -> void {
    auto argv = std::vector<char*> {};
    argv.reserve(command.size() + 1);

    for (const auto& argument : command) {
        argv.push_back(const_cast<char*>(argument.c_str()));
    }

    argv.push_back(nullptr);

    const auto exit_code = _spawnvp(_P_WAIT, command.front().c_str(), argv.data());
    test_support::expect_true(exit_code == 0, "argv command should succeed");
}

auto probe_duration_ms(const std::filesystem::path& file_path) -> u64 {
    const auto output =
        run_capture("ffprobe -v error -show_entries format=duration -of default=nokey=1:noprint_wrappers=1 "
            + quote(file_path.string()));
    const auto seconds = std::stod(output);
    return static_cast<u64>(seconds * 1000.0);
}

} // namespace

auto main() -> int {
    const auto root = std::filesystem::temp_directory_path() / "vidchopper-integration";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto source_path = root / "sample.mp4";
    run_args_no_capture({
        "ffmpeg",
        "-y",
        "-f",
        "lavfi",
        "-i",
        "testsrc=size=320x180:rate=24",
        "-f",
        "lavfi",
        "-i",
        "sine=frequency=440:sample_rate=48000",
        "-t",
        "6",
        "-c:v",
        "libx264",
        "-pix_fmt",
        "yuv420p",
        "-c:a",
        "aac",
        source_path.string(),
    });

    const auto metadata = VideoMetadata {
        .source_path = source_path,
        .duration_ms = 6000,
        .frame_rate = FrameRate {.numerator = 24, .denominator = 1},
        .source_extension = ".mp4",
    };
    auto settings = ExportSettings {};
    settings.ffmpeg_path = "ffmpeg";
    settings.naming_pattern = "%index%_%name%";
    settings.overwrite_mode = OverwriteMode::Overwrite;
    settings.seek_mode = SeekMode::Fast;
    settings.verify_output_durations = false;
    settings.write_json_manifest = false;
    settings.write_csv_manifest = false;

    const auto chapters = std::vector<ChapterSegment> {
        {.name = "Intro", .start_ms = 0, .end_ms = 2000},
        {.name = "Outro", .start_ms = 2000, .end_ms = 4000},
    };

    const auto output_directory = root / "out";
    std::filesystem::create_directories(output_directory);

    const auto environment = EncoderEnvironment {};
    for (auto index = size_t {0}; index < chapters.size(); ++index) {
        const auto chapter_index = static_cast<u16>(index);
        const auto output_path = output_path_for(metadata, chapters[index], chapter_index, output_directory, settings);
        const auto command = build_ffmpeg_command(metadata, chapters[index], output_path, settings, environment);
        run_args_no_capture(command);
        test_support::expect_true(std::filesystem::exists(output_path), "expected output file to be created");
        const auto duration_ms = probe_duration_ms(output_path);
        test_support::expect_true(duration_ms >= 1500 && duration_ms <= 2500, "output duration should be about 2s");
    }

    return 0;
}
