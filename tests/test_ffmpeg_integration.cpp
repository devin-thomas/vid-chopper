#include "core/path_utils.hpp"
#include "services/export_engine.hpp"
#include "services/export_planner.hpp"
#include "services/manifest_writer.hpp"
#include "services/probe_service.hpp"
#include "services/process_runner.hpp"
#include "test_support.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace vidchopper;

namespace {

class TemporaryRoot final {
public:
    explicit TemporaryRoot(Path path)
        : path_ {std::move(path)} {
    }
    TemporaryRoot(const TemporaryRoot&) = delete;
    auto operator=(const TemporaryRoot&) -> TemporaryRoot& = delete;
    ~TemporaryRoot() {
        auto error = std::error_code {};
        std::filesystem::remove_all(path_, error);
    }

    [[nodiscard]] auto path() const -> const Path& {
        return path_;
    }

private:
    Path path_;
};

[[nodiscard]] auto run_command(const std::vector<std::string>& command) -> ProcessResult {
    test_support::expect_true(!command.empty(), "integration command should contain an executable");
    return run_process(ProcessRequest {
        .executable = path_from_utf8(command.front()),
        .arguments = {command.begin() + 1, command.end()},
        .timeout = std::chrono::minutes {2},
        .stdout_limit_bytes = 128 * 1024,
        .stderr_limit_bytes = 16 * 1024,
    });
}

[[nodiscard]] auto read_text(const Path& path) -> std::string {
    auto stream = std::ifstream {path, std::ios::binary};
    test_support::expect_true(stream.is_open(), "integration fixture output should be readable");
    auto output = std::ostringstream {};
    output << stream.rdbuf();
    return output.str();
}

[[nodiscard]] auto json_string_literal(const std::string_view value) -> std::string {
    auto escaped = std::string {"\""};
    for (const char character : value) {
        switch (character) {
        case '\\':
            escaped += "\\\\";
            break;
        case '\"':
            escaped += "\\\"";
            break;
        default:
            escaped.push_back(character);
            break;
        }
    }
    escaped.push_back('\"');
    return escaped;
}

[[nodiscard]] auto probe_duration_ms(const Path& file_path) -> u64 {
    const ProcessResult process = run_process(ProcessRequest {
        .executable = path_from_utf8("ffprobe"),
        .arguments = {"-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "default=nokey=1:noprint_wrappers=1",
            path_to_utf8(file_path)},
        .timeout = std::chrono::minutes {2},
    });
    test_support::expect_eq(process.state, ProcessExitState::Success, "ffprobe duration command should succeed");
    return static_cast<u64>(std::stod(process.standard_output) * 1000.0);
}

auto expect_success(const ProcessResult& process, const std::string_view message) -> void {
    test_support::expect_eq(process.state, ProcessExitState::Success, message);
}

} // namespace

auto main() -> int {
    const TemporaryRoot temporary_root {
        std::filesystem::temp_directory_path() / path_from_utf8("vidchopper integration fixtures")};
    const Path root = temporary_root.path();
    std::filesystem::create_directories(root);

    const Path source_path = root / path_from_utf8("source clips/源 sample 🎬.mp4");
    std::filesystem::create_directories(source_path.parent_path());
    expect_success(run_command({"ffmpeg",
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
                       path_to_utf8(source_path)}),
        "real ffmpeg fixture generation should succeed");
    test_support::expect_true(std::filesystem::exists(source_path), "generated source fixture should exist");

    const ProbeResult probed = ProbeService {}.probe(path_from_utf8("ffprobe"), source_path);
    test_support::expect_true(probed.ok(), "real ffprobe should parse the generated source");
    test_support::expect_eq(probed.metadata.source_path, source_path, "probe should retain a Unicode source path");
    test_support::expect_true(probed.metadata.duration_ms >= 5500 && probed.metadata.duration_ms <= 6500,
        "probe should report the generated source duration");

    auto settings = ExportSettings {};
    settings.ffmpeg_path = "ffmpeg";
    settings.ffprobe_path = "ffprobe";
    settings.output_folder_pattern = "output";
    settings.naming_pattern = "%index%_%name%";
    settings.encoder_kind = EncoderKind::X264;
    settings.x264_crf = 23;
    settings.overwrite_mode = OverwriteMode::Overwrite;
    settings.seek_mode = SeekMode::Fast;
    settings.verify_output_durations = true;
    settings.write_json_manifest = true;
    settings.write_csv_manifest = true;

    const Path output_directory = root / path_from_utf8("output clips/章 exports 🎬");
    const auto chapters = std::vector<ChapterSegment> {
        {.name = "序章 Intro", .start_ms = 0, .end_ms = 2000},
        {.name = "Outro spaced", .start_ms = 2000, .end_ms = 4000},
    };
    const OutputPlanResult planned = plan_outputs({OutputPlanInput {
        .metadata = probed.metadata,
        .output_directory = output_directory,
        .chapters = chapters,
        .settings = settings,
        .environment = EncoderEnvironment {},
    }});
    test_support::expect_true(planned.ok(), "Unicode export fixture should plan successfully");
    test_support::expect_eq(planned.jobs.size(), size_t {1}, "one real export job should be planned");

    const ExportRunResult exported = ExportEngine {}.run(planned.jobs,
        ExportRunOptions {
            .process_timeout = std::chrono::minutes {2},
            .stdout_limit_bytes = 128 * 1024,
            .stderr_limit_bytes = 16 * 1024,
        });
    test_support::expect_true(exported.ok(), "real x264 export should succeed");
    test_support::expect_eq(exported.jobs.size(), size_t {1}, "real export should retain one job result");
    test_support::expect_eq(
        exported.jobs.front().segments.size(), chapters.size(), "real export should render every planned chapter");

    for (const RenderedSegment& segment : exported.jobs.front().segments) {
        test_support::expect_true(
            std::filesystem::exists(segment.output_path), "real export should create each Unicode/spaced output path");
        const u64 duration_ms = probe_duration_ms(segment.output_path);
        test_support::expect_true(
            duration_ms >= 1500 && duration_ms <= 2500, "real output duration should be about two seconds");
    }

    const Path aggregate_json = root / path_from_utf8("aggregate manifests/汇总 🎬.json");
    const Path aggregate_csv = root / path_from_utf8("aggregate manifests/汇总 🎬.csv");
    const ManifestWriteResult manifests = write_manifests(planned.jobs, exported, aggregate_json, aggregate_csv);
    test_support::expect_true(manifests.ok(), "real export manifests should be written successfully");
    const std::string json_text = read_text(output_directory / "vidchopper-manifest.json");
    const std::string csv_text = read_text(output_directory / "vidchopper-manifest.csv");
    test_support::expect_true(json_text.find("\"source\": " + json_string_literal(path_to_utf8(source_path)))
            != std::string::npos,
        "JSON manifest should preserve the Unicode source path");
    test_support::expect_true(
        json_text.find("\"outputDirectory\": " + json_string_literal(path_to_utf8(output_directory)))
            != std::string::npos,
        "JSON manifest should preserve the spaced output directory");
    test_support::expect_true(json_text.find(json_string_literal("序章 Intro")) != std::string::npos,
        "JSON manifest should preserve the Unicode chapter name");
    test_support::expect_true(
        csv_text.find(path_to_utf8(planned.jobs.front().segments.front().output_path)) != std::string::npos,
        "CSV manifest should preserve the Unicode/spaced output path");
    test_support::expect_true(std::filesystem::exists(aggregate_json), "aggregate JSON should be written");
    test_support::expect_true(std::filesystem::exists(aggregate_csv), "aggregate CSV should be written");

    return 0;
}
