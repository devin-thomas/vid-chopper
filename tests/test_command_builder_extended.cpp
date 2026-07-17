#include "core/command_builder.hpp"
#include "core/chapter_plan.hpp"
#include "test_support.hpp"

#include <algorithm>

using namespace vidchopper;

namespace {

auto contains(const std::vector<std::string>& vec, const std::string& value) -> bool {
    return std::find(vec.begin(), vec.end(), value) != vec.end();
}

auto make_metadata() -> VideoMetadata {
    return VideoMetadata {
        .source_path = "C:/media/project.mp4",
        .duration_ms = 60000,
        .frame_rate = {.numerator = 30, .denominator = 1},
        .embedded_chapters = {},
        .source_extension = ".mp4",
    };
}

auto make_chapter() -> ChapterSegment {
    return ChapterSegment {
        .name = "Introduction",
        .start_ms = 5000,
        .end_ms = 20000,
    };
}

} // namespace

auto main() -> int {
    // output_extension_for: ContainerMode::Mp4
    {
        auto metadata = make_metadata();
        auto settings = ExportSettings {};
        settings.container_mode = ContainerMode::Mp4;
        test_support::expect_eq(
            output_extension_for(metadata, settings), std::string {".mp4"}, "Mp4 mode should return .mp4");
    }

    // output_extension_for: ContainerMode::Mkv
    {
        auto metadata = make_metadata();
        auto settings = ExportSettings {};
        settings.container_mode = ContainerMode::Mkv;
        test_support::expect_eq(
            output_extension_for(metadata, settings), std::string {".mkv"}, "Mkv mode should return .mkv");
    }

    // output_extension_for: ContainerMode::Source preserves common extensions
    {
        auto metadata = make_metadata();
        metadata.source_extension = ".MOV";
        auto settings = ExportSettings {};
        settings.container_mode = ContainerMode::Source;
        test_support::expect_eq(
            output_extension_for(metadata, settings), std::string {".mov"}, "Source mode should lowercase .MOV");
    }

    // output_extension_for: Source mode falls back to .mp4 for uncommon extensions
    {
        auto metadata = make_metadata();
        metadata.source_extension = ".avi";
        auto settings = ExportSettings {};
        settings.container_mode = ContainerMode::Source;
        test_support::expect_eq(output_extension_for(metadata, settings),
            std::string {".mp4"},
            "Source mode should fall back to .mp4 for unsupported extension");
    }

    // output_extension_for: Source mode uses path extension when source_extension is empty
    {
        auto metadata = make_metadata();
        metadata.source_extension = "";
        metadata.source_path = "video.mkv";
        auto settings = ExportSettings {};
        settings.container_mode = ContainerMode::Source;
        test_support::expect_eq(output_extension_for(metadata, settings),
            std::string {".mkv"},
            "Source mode should use path extension as fallback");
    }

    // output_extension_for: Source mode defaults to .mp4 when both are empty
    {
        auto metadata = make_metadata();
        metadata.source_extension = "";
        metadata.source_path = "video";
        auto settings = ExportSettings {};
        settings.container_mode = ContainerMode::Source;
        test_support::expect_eq(output_extension_for(metadata, settings),
            std::string {".mp4"},
            "Source mode should default to .mp4 when no extension");
    }

    // resolve_encoder: explicit HevcNvenc setting
    {
        auto settings = ExportSettings {};
        settings.encoder_kind = EncoderKind::HevcNvenc;
        const auto encoder = resolve_encoder(settings, EncoderEnvironment {});
        test_support::expect_eq(
            encoder.video_codec, std::string {"hevc_nvenc"}, "explicit HevcNvenc should always select hevc_nvenc");
        test_support::expect_eq(encoder.kind, EncoderKind::HevcNvenc, "encoder kind should be HevcNvenc");
    }

    // resolve_encoder: Auto without GPU falls back to x264
    {
        auto settings = ExportSettings {};
        settings.encoder_kind = EncoderKind::Auto;
        settings.auto_detect_gpu = true;
        const auto encoder =
            resolve_encoder(settings, EncoderEnvironment {.has_nvidia_gpu = false, .has_hevc_nvenc_encoder = false});
        test_support::expect_eq(
            encoder.video_codec, std::string {"libx264"}, "Auto without GPU should fall back to libx264");
    }

    // resolve_encoder: Auto with GPU but auto_detect_gpu disabled
    {
        auto settings = ExportSettings {};
        settings.encoder_kind = EncoderKind::Auto;
        settings.auto_detect_gpu = false;
        const auto encoder =
            resolve_encoder(settings, EncoderEnvironment {.has_nvidia_gpu = true, .has_hevc_nvenc_encoder = true});
        test_support::expect_eq(
            encoder.video_codec, std::string {"libx264"}, "Auto with GPU detection disabled should use libx264");
    }

    // resolve_encoder: Auto with GPU but no hevc_nvenc encoder
    {
        auto settings = ExportSettings {};
        settings.encoder_kind = EncoderKind::Auto;
        settings.auto_detect_gpu = true;
        const auto encoder =
            resolve_encoder(settings, EncoderEnvironment {.has_nvidia_gpu = true, .has_hevc_nvenc_encoder = false});
        test_support::expect_eq(
            encoder.video_codec, std::string {"libx264"}, "Auto with GPU but no nvenc encoder should use libx264");
    }

    // resolve_encoder: custom preset and quality settings
    {
        auto settings = ExportSettings {};
        settings.encoder_kind = EncoderKind::X264;
        settings.x264_preset = "ultrafast";
        settings.x264_crf = 28;
        const auto encoder = resolve_encoder(settings, EncoderEnvironment {});
        test_support::expect_true(contains(encoder.arguments, "ultrafast"), "x264 encoder should use custom preset");
        test_support::expect_true(contains(encoder.arguments, "28"), "x264 encoder should use custom crf");
    }

    // build_ffmpeg_command: fast seek mode places -ss before -i
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.seek_mode = SeekMode::Fast;
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        auto ss_pos = std::find(cmd.begin(), cmd.end(), "-ss");
        auto i_pos = std::find(cmd.begin(), cmd.end(), "-i");
        test_support::expect_true(ss_pos != cmd.end(), "fast seek should include -ss");
        test_support::expect_true(ss_pos < i_pos, "fast seek should place -ss before -i");
    }

    // build_ffmpeg_command: accurate seek mode places -ss after -i
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.seek_mode = SeekMode::Accurate;
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        auto ss_pos = std::find(cmd.begin(), cmd.end(), "-ss");
        auto i_pos = std::find(cmd.begin(), cmd.end(), "-i");
        test_support::expect_true(ss_pos != cmd.end(), "accurate seek should include -ss");
        test_support::expect_true(ss_pos > i_pos, "accurate seek should place -ss after -i");
    }

    // build_ffmpeg_command: audio copy mode
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.audio_mode = AudioMode::Copy;
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        auto ca_pos = std::find(cmd.begin(), cmd.end(), "-c:a");
        test_support::expect_true(ca_pos != cmd.end(), "should include audio codec flag");
        test_support::expect_eq(*(ca_pos + 1), std::string {"copy"}, "audio copy mode should use 'copy'");
    }

    // build_ffmpeg_command: aac audio mode with bitrate
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.audio_mode = AudioMode::Aac;
        settings.aac_bitrate_kbps = 256;
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        test_support::expect_true(contains(cmd, "aac"), "aac audio mode should specify aac codec");
        test_support::expect_true(contains(cmd, "256k"), "aac mode should specify bitrate");
    }

    // build_ffmpeg_command: skip mode (-n flag)
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.overwrite_mode = OverwriteMode::Skip;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        test_support::expect_true(contains(cmd, "-n"), "skip overwrite mode should use -n flag");
        test_support::expect_true(!contains(cmd, "-y"), "skip overwrite mode should not use -y flag");
    }

    // build_ffmpeg_command: threads setting
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.ffmpeg_threads = 4;
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        test_support::expect_true(contains(cmd, "-threads"), "non-zero threads should include -threads flag");
        test_support::expect_true(contains(cmd, "4"), "threads value should be present");
    }

    // build_ffmpeg_command: zero threads omits flag
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.ffmpeg_threads = 0;
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        test_support::expect_true(!contains(cmd, "-threads"), "zero threads should omit -threads flag");
    }

    // build_ffmpeg_command: copy_source_metadata=false strips metadata
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.copy_source_metadata = false;
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        auto map_metadata_pos = std::find(cmd.begin(), cmd.end(), "-map_metadata");
        test_support::expect_true(map_metadata_pos != cmd.end(), "should include -map_metadata");
        test_support::expect_eq(
            *(map_metadata_pos + 1), std::string {"-1"}, "copy_source_metadata=false should map to -1");
    }

    // build_ffmpeg_command: copy_source_metadata=true copies metadata
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.copy_source_metadata = true;
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        auto map_metadata_pos = std::find(cmd.begin(), cmd.end(), "-map_metadata");
        test_support::expect_true(map_metadata_pos != cmd.end(), "should include -map_metadata");
        test_support::expect_eq(
            *(map_metadata_pos + 1), std::string {"0"}, "copy_source_metadata=true should map to 0");
    }

    // build_ffmpeg_command: mp4 output adds movflags
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        test_support::expect_true(contains(cmd, "-movflags"), "mp4 output should include -movflags");
        test_support::expect_true(contains(cmd, "+faststart"), "mp4 output should include +faststart");
    }

    // build_ffmpeg_command: non-mp4 output omits movflags
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mkv"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        test_support::expect_true(!contains(cmd, "-movflags"), "mkv output should not include -movflags");
    }

    // build_ffmpeg_command: custom ffmpeg path
    {
        auto metadata = make_metadata();
        auto chapter = make_chapter();
        auto settings = ExportSettings {};
        settings.ffmpeg_path = "C:/tools/ffmpeg.exe";
        settings.overwrite_mode = OverwriteMode::Overwrite;
        const auto output = std::filesystem::path {"output.mp4"};
        const auto cmd = build_ffmpeg_command(metadata, chapter, output, settings, EncoderEnvironment {});
        test_support::expect_eq(
            cmd.front(), std::string {"C:/tools/ffmpeg.exe"}, "command should use custom ffmpeg path");
    }

    // output_path_for: sanitize_file_names=false skips per-name sanitization but final filename is still sanitized
    {
        auto metadata = make_metadata();
        auto chapter_empty = ChapterSegment {.name = "", .start_ms = 0, .end_ms = 5000};
        auto settings_on = ExportSettings {};
        settings_on.sanitize_file_names = true;
        settings_on.container_mode = ContainerMode::Mp4;
        const auto output_on = output_path_for(metadata, chapter_empty, 0, "/output", settings_on);

        auto settings_off = ExportSettings {};
        settings_off.sanitize_file_names = false;
        settings_off.container_mode = ContainerMode::Mp4;
        const auto output_off = output_path_for(metadata, chapter_empty, 0, "/output", settings_off);

        const auto name_on = output_on.filename().string();
        const auto name_off = output_off.filename().string();
        test_support::expect_true(name_on.find("chapter") != std::string::npos,
            "sanitize=true with empty name should use 'chapter' fallback");
        test_support::expect_true(name_on != name_off, "sanitize on/off should differ for empty chapter name");
    }

    // output_path_for: custom naming pattern with %source%
    {
        auto metadata = make_metadata();
        metadata.source_path = "C:/media/my-video.mp4";
        auto chapter = ChapterSegment {.name = "Outro", .start_ms = 50000, .end_ms = 60000};
        auto settings = ExportSettings {};
        settings.naming_pattern = "%source%_%index%_%name%";
        settings.container_mode = ContainerMode::Mp4;
        const auto output = output_path_for(metadata, chapter, 2, "/output", settings);
        const auto filename = output.filename().string();
        test_support::expect_true(
            filename.find("my-video") != std::string::npos, "naming pattern should substitute %source%");
        test_support::expect_true(
            filename.find("03") != std::string::npos, "naming pattern should substitute %index% (1-based)");
        test_support::expect_true(
            filename.find("Outro") != std::string::npos, "naming pattern should substitute %name%");
    }

    return 0;
}
