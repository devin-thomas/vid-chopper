#include "core/command_builder.hpp"

#include "core/string_utils.hpp"
#include "core/timecode.hpp"

#include <initializer_list>
#include <string_view>
#include <utility>

namespace vidchopper {

// Centralised ffmpeg flag names so the long command builder cannot drift on a
// mistyped literal and future refactors have a single source of truth.
namespace ffmpeg_arg {

constexpr auto overwrite = std::string_view {"-y"};
constexpr auto no_overwrite = std::string_view {"-n"};
constexpr auto seek = std::string_view {"-ss"};
constexpr auto input = std::string_view {"-i"};
constexpr auto duration = std::string_view {"-t"};
constexpr auto video_codec = std::string_view {"-c:v"};
constexpr auto audio_codec = std::string_view {"-c:a"};
constexpr auto audio_bitrate = std::string_view {"-b:a"};
constexpr auto threads = std::string_view {"-threads"};
constexpr auto map_metadata = std::string_view {"-map_metadata"};
constexpr auto metadata = std::string_view {"-metadata"};
constexpr auto movflags = std::string_view {"-movflags"};

} // namespace ffmpeg_arg

namespace {

// Append a run of tokens in one call, replacing the long paired emplace_back
// sequences. string_view arguments bind to both the ffmpeg_arg constants and
// freshly built std::string values without forcing the caller to spell types.
auto append(std::vector<std::string>& command, std::initializer_list<std::string_view> tokens) -> void {
    for (const std::string_view token : tokens) {
        command.emplace_back(token);
    }
}

auto safe_source_extension(const VideoMetadata& metadata) -> std::string {
    std::string extension = metadata.source_extension;
    if (extension.empty()) {
        extension = metadata.source_path.extension().string();
    }

    if (extension.empty()) {
        return ".mp4";
    }

    extension = to_lower_copy(std::move(extension));

    if (extension != ".mp4" && extension != ".mkv" && extension != ".mov") {
        return ".mp4";
    }

    return extension;
}

auto append_audio_arguments(std::vector<std::string>& command, const ExportSettings& settings) -> void {
    if (settings.audio_mode == AudioMode::Copy) {
        append(command, {ffmpeg_arg::audio_codec, "copy"});
        return;
    }

    const std::string bitrate = std::to_string(settings.aac_bitrate_kbps) + "k";
    append(command, {ffmpeg_arg::audio_codec, "aac"});
    append(command, {ffmpeg_arg::audio_bitrate, bitrate});
}

} // namespace

auto resolve_encoder(const ExportSettings& settings, const EncoderEnvironment& environment) -> ResolvedEncoder {
    const bool use_nvenc = settings.encoder_kind == EncoderKind::HevcNvenc
        || (settings.encoder_kind == EncoderKind::Auto && settings.auto_detect_gpu && environment.has_nvidia_gpu
            && environment.has_hevc_nvenc_encoder);

    if (use_nvenc) {
        return ResolvedEncoder {
            .kind = EncoderKind::HevcNvenc,
            .video_codec = "hevc_nvenc",
            .arguments = {"-preset", settings.nvenc_preset, "-cq", std::to_string(settings.nvenc_cq), "-rc", "vbr_hq"},
        };
    }

    return ResolvedEncoder {
        .kind = EncoderKind::X264,
        .video_codec = "libx264",
        .arguments = {"-preset", settings.x264_preset, "-crf", std::to_string(settings.x264_crf)},
    };
}

auto output_extension_for(const VideoMetadata& metadata, const ExportSettings& settings) -> std::string {
    switch (settings.container_mode) {
    case ContainerMode::Mp4:
        return ".mp4";
    case ContainerMode::Mkv:
        return ".mkv";
    case ContainerMode::Source:
    default:
        return safe_source_extension(metadata);
    }
}

auto output_path_for(const VideoMetadata& metadata,
    const ChapterSegment& chapter,
    const u16 chapter_index,
    const Path& output_directory,
    const ExportSettings& settings) -> Path {
    std::string chapter_name = trim_copy(chapter.name);
    if (settings.sanitize_file_names) {
        chapter_name = sanitize_file_component(chapter.name);
    }

    std::string file_name = replace_all_copy(settings.naming_pattern, "%name%", chapter_name);
    file_name = replace_all_copy(file_name, "%source%", metadata.source_path.stem().string());

    const std::string index_text = zero_padded_index(static_cast<u16>(chapter_index + 1), settings.index_padding);
    file_name = replace_all_copy(file_name, "%index%", index_text);
    file_name = sanitize_file_component(file_name);

    return output_directory / (file_name + output_extension_for(metadata, settings));
}

auto build_ffmpeg_command(const VideoMetadata& metadata,
    const ChapterSegment& chapter,
    const Path& output_path,
    const ExportSettings& settings,
    const EncoderEnvironment& environment) -> std::vector<std::string> {
    auto command = std::vector<std::string> {};
    command.reserve(32);
    command.emplace_back(settings.ffmpeg_path);

    append(command,
        {settings.overwrite_mode == OverwriteMode::Overwrite ? ffmpeg_arg::overwrite : ffmpeg_arg::no_overwrite});

    const std::string start_time = format_millisecond_timecode(chapter.start_ms);
    const u64 duration_ms = chapter.end_ms - chapter.start_ms;
    const std::string duration = format_millisecond_timecode(duration_ms);

    if (settings.seek_mode == SeekMode::Fast) {
        append(command, {ffmpeg_arg::seek, start_time});
    }

    append(command, {ffmpeg_arg::input, metadata.source_path.string()});

    if (settings.seek_mode == SeekMode::Accurate) {
        append(command, {ffmpeg_arg::seek, start_time});
    }

    append(command, {ffmpeg_arg::duration, duration});

    const ResolvedEncoder resolved_encoder = resolve_encoder(settings, environment);
    append(command, {ffmpeg_arg::video_codec, resolved_encoder.video_codec});
    command.insert(command.end(), resolved_encoder.arguments.begin(), resolved_encoder.arguments.end());

    if (settings.ffmpeg_threads > 0) {
        append(command, {ffmpeg_arg::threads, std::to_string(settings.ffmpeg_threads)});
    }

    append_audio_arguments(command, settings);

    append(command, {ffmpeg_arg::map_metadata, settings.copy_source_metadata ? "0" : "-1"});
    append(command, {ffmpeg_arg::metadata, "title=" + trim_copy(chapter.name)});

    if (output_path.extension() == ".mp4") {
        append(command, {ffmpeg_arg::movflags, "+faststart"});
    }

    const std::vector<std::string> extra_arguments = split_quoted_arguments(settings.extra_ffmpeg_args);
    command.insert(command.end(), extra_arguments.begin(), extra_arguments.end());
    command.emplace_back(output_path.string());

    return command;
}

} // namespace vidchopper
