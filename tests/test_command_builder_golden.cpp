#include "core/command_builder.h"
#include "test_support.h"

#include <string>
#include <vector>

using namespace vidchopper;

namespace {

auto joined(const std::vector<std::string>& command) -> std::string {
    auto out = std::string {};
    for (const auto& token : command) {
        out += token;
        out.push_back('\n');
    }
    return out;
}

} // namespace

// Golden tests locking the byte-identical ffmpeg argument list across the
// command-builder modernization (append() helper + ffmpeg_arg constants).
// Each scenario pins the exact ordered token stream the legacy paired
// emplace_back implementation produced.
auto main() -> int {
    const auto metadata = VideoMetadata {
        .source_path = "C:/media/demo-video.mov",
        .duration_ms = 120000,
        .frame_rate = {.numerator = 24, .denominator = 1},
        .source_extension = ".mov",
    };

    const auto chapter = ChapterSegment {
        .name = "Intro / Setup",
        .start_ms = 5000,
        .end_ms = 15000,
    };

    // Scenario 1: x264, accurate seek, aac audio, copy metadata, mp4 output.
    {
        auto settings = ExportSettings {};
        settings.encoder_kind = EncoderKind::X264;
        settings.overwrite_mode = OverwriteMode::Overwrite;
        settings.seek_mode = SeekMode::Accurate;
        settings.audio_mode = AudioMode::Aac;
        settings.container_mode = ContainerMode::Mp4;
        settings.ffmpeg_threads = 4;
        settings.extra_ffmpeg_args = "-pix_fmt yuv420p";

        const auto command =
            build_ffmpeg_command(metadata, chapter, "C:/out/clip.mp4", settings, EncoderEnvironment {});

        const auto expected = std::string {"ffmpeg\n-y\n-i\nC:/media/demo-video.mov\n-ss\n00:00:05.000\n-t\n"
                                           "00:00:10.000\n-c:v\nlibx264\n-preset\nslow\n-crf\n18\n-threads\n4\n"
                                           "-c:a\naac\n-b:a\n192k\n-map_metadata\n0\n-metadata\ntitle=Intro / Setup\n"
                                           "-movflags\n+faststart\n-pix_fmt\nyuv420p\nC:/out/clip.mp4\n"};
        test_support::expect_eq(joined(command), expected, "x264 accurate aac mp4 command");
    }

    // Scenario 2: nvenc, fast seek, copy audio, strip metadata, mkv output.
    {
        auto settings = ExportSettings {};
        settings.encoder_kind = EncoderKind::HevcNvenc;
        settings.overwrite_mode = OverwriteMode::Skip;
        settings.seek_mode = SeekMode::Fast;
        settings.audio_mode = AudioMode::Copy;
        settings.container_mode = ContainerMode::Mkv;
        settings.copy_source_metadata = false;
        settings.ffmpeg_threads = 0;

        const auto command = build_ffmpeg_command(metadata,
            chapter,
            "C:/out/clip.mkv",
            settings,
            EncoderEnvironment {.has_nvidia_gpu = true, .has_hevc_nvenc_encoder = true});

        const auto expected = std::string {"ffmpeg\n-n\n-ss\n00:00:05.000\n-i\nC:/media/demo-video.mov\n-t\n"
                                           "00:00:10.000\n-c:v\nhevc_nvenc\n-preset\np5\n-cq\n22\n-rc\nvbr_hq\n"
                                           "-c:a\ncopy\n-map_metadata\n-1\n-metadata\ntitle=Intro / Setup\n"
                                           "C:/out/clip.mkv\n"};
        test_support::expect_eq(joined(command), expected, "nvenc fast copy mkv command");
    }

    return 0;
}
