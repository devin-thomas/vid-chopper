#pragma once

#include "core/types.h"

#include <string>
#include <vector>

namespace vidchopper {

enum class TimestampDisplayMode : bool {
    Milliseconds = false,
    Frames = true,
};

enum class EncoderKind : u8 {
    Auto = 0,
    X264 = 1,
    HevcNvenc = 2,
};

enum class AudioMode : bool {
    Copy = false,
    Aac = true,
};

enum class ContainerMode : u8 {
    Source = 0,
    Mp4 = 1,
    Mkv = 2,
};

enum class OverwriteMode : u8 {
    Ask = 0,
    Overwrite = 1,
    Skip = 2,
};

enum class SeekMode : bool {
    Accurate = false,
    Fast = true,
};

struct FrameRate {
    u32 numerator {0};
    u32 denominator {1};

    [[nodiscard]] constexpr auto valid() const noexcept -> bool;
    [[nodiscard]] constexpr auto as_f64() const noexcept -> f64;
    [[nodiscard]] constexpr auto display_frames_per_second() const noexcept -> u32;

    [[nodiscard]] auto operator<=>(const FrameRate&) const = default;
};

struct ChapterSegment {
    std::string name;
    u64 start_ms {0};
    u64 end_ms {0};

    [[nodiscard]] auto operator<=>(const ChapterSegment&) const = default;
};

struct VideoMetadata {
    Path source_path;
    u64 duration_ms {0};
    FrameRate frame_rate {};
    std::vector<ChapterSegment> embedded_chapters;
    std::string source_extension;

    [[nodiscard]] auto operator==(const VideoMetadata&) const -> bool = default;
};

struct EncoderEnvironment {
    bool has_nvidia_gpu {false};
    bool has_hevc_nvenc_encoder {false};

    [[nodiscard]] auto operator==(const EncoderEnvironment&) const -> bool = default;
};

struct ExportSettings {
    std::string ffmpeg_path {"ffmpeg"};
    std::string ffprobe_path {"ffprobe"};
    std::string output_folder_pattern {"%source%_chapters"};
    std::string naming_pattern {"%index% - %name%"};
    std::string x264_preset {"slow"};
    std::string nvenc_preset {"p5"};
    std::string extra_ffmpeg_args;

    EncoderKind encoder_kind {EncoderKind::Auto};
    AudioMode audio_mode {AudioMode::Copy};
    ContainerMode container_mode {ContainerMode::Source};
    OverwriteMode overwrite_mode {OverwriteMode::Ask};
    SeekMode seek_mode {SeekMode::Accurate};
    TimestampDisplayMode display_mode {TimestampDisplayMode::Milliseconds};

    u8 default_chapter_count {6};
    u8 max_chapters {255};
    u8 index_padding {2};
    u8 x264_crf {18};
    u8 nvenc_cq {22};
    u8 min_chapter_seconds {1};
    u8 ffmpeg_threads {0};

    u16 aac_bitrate_kbps {192};

    bool auto_detect_gpu {true};
    bool open_output_directory_after_export {true};
    bool sanitize_file_names {true};
    bool stop_on_first_error {true};
    bool write_json_manifest {true};
    bool write_csv_manifest {false};
    bool verify_output_durations {true};
    bool copy_source_metadata {true};
    bool prefer_embedded_chapters {true};
    bool confirm_remove_chapters {true};
    bool confirm_exit {true};

    [[nodiscard]] auto operator==(const ExportSettings&) const -> bool = default;
};

struct ResolvedEncoder {
    EncoderKind kind {EncoderKind::X264};
    std::string video_codec;
    std::vector<std::string> arguments;

    [[nodiscard]] auto operator==(const ResolvedEncoder&) const -> bool = default;
};

constexpr auto FrameRate::valid() const noexcept -> bool {
    return numerator > 0 && denominator > 0;
}

constexpr auto FrameRate::as_f64() const noexcept -> f64 {
    if (!valid()) {
        return 0.0;
    }

    return static_cast<f64>(numerator) / static_cast<f64>(denominator);
}

constexpr auto FrameRate::display_frames_per_second() const noexcept -> u32 {
    if (!valid()) {
        return 0;
    }

    // std::lround is not constexpr before C++23; frame rates are always positive,
    // so round-half-up via truncation is correct and keeps this function constexpr.
    // NOLINTNEXTLINE(bugprone-incorrect-roundings)
    const u32 rounded = static_cast<u32>(as_f64() + 0.5);
    return rounded == 0 ? 1 : rounded;
}

} // namespace vidchopper
