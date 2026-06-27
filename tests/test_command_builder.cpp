#include "core/chapter_plan.h"
#include "core/command_builder.h"
#include "test_support.h"

#include <algorithm>
#include <ranges>

using namespace vidchopper;

auto main() -> int {
    const auto metadata = VideoMetadata {
        .source_path = "C:/media/demo-video.mov",
        .duration_ms = 120000,
        .frame_rate = {.numerator = 24, .denominator = 1},
        .source_extension = ".mov",
    };

    const auto chapter = ChapterSegment {
        .name = "Intro / Setup",
        .start_ms = 0,
        .end_ms = 10000,
    };

    auto settings = ExportSettings {};
    settings.overwrite_mode = OverwriteMode::Overwrite;
    settings.audio_mode = AudioMode::Aac;
    settings.extra_ffmpeg_args = "-pix_fmt yuv420p \"-metadata comment=demo clip\"";

    const auto output_directory = default_output_directory(metadata.source_path, settings);
    const auto output_path = output_path_for(metadata, chapter, 0, output_directory, settings);
    test_support::expect_eq(output_path.filename().string(),
        std::string {"01 - Intro _ Setup.mov"},
        "output name should be sanitized and padded");

    const auto auto_encoder =
        resolve_encoder(settings, EncoderEnvironment {.has_nvidia_gpu = true, .has_hevc_nvenc_encoder = true});
    test_support::expect_eq(
        auto_encoder.video_codec, std::string {"hevc_nvenc"}, "auto mode should prefer nvenc when available");

    settings.encoder_kind = EncoderKind::X264;
    const auto x264_encoder =
        resolve_encoder(settings, EncoderEnvironment {.has_nvidia_gpu = true, .has_hevc_nvenc_encoder = true});
    test_support::expect_eq(
        x264_encoder.video_codec, std::string {"libx264"}, "explicit x264 mode should override nvenc");

    const auto command = build_ffmpeg_command(metadata, chapter, output_path, settings, EncoderEnvironment {});
    test_support::expect_eq(command.front(), std::string {"ffmpeg"}, "command should start with ffmpeg");
    test_support::expect_true(std::ranges::find(command, "libx264") != command.end(), "x264 codec should be selected");
    test_support::expect_true(std::ranges::find(command, "aac") != command.end(), "aac audio codec should be selected");
    test_support::expect_true(
        std::ranges::find(command, "-metadata") != command.end(), "command should include chapter title metadata");
    test_support::expect_true(
        std::ranges::find(command, "-pix_fmt") != command.end(), "extra ffmpeg args should be appended");
    test_support::expect_true(
        command.back().ends_with(".mov"), "source container mode should preserve common source extension");

    return 0;
}
