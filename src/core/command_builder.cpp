#include "core/command_builder.h"

#include "core/string_utils.h"
#include "core/timecode.h"

namespace vidchopper {

namespace {

auto safe_source_extension(const VideoMetadata& metadata) -> std::string {
    auto extension = metadata.source_extension;
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
        command.emplace_back("-c:a");
        command.emplace_back("copy");
        return;
    }

    command.emplace_back("-c:a");
    command.emplace_back("aac");
    command.emplace_back("-b:a");
    command.emplace_back(std::to_string(settings.aac_bitrate_kbps) + "k");
}

} // namespace

auto resolve_encoder(const ExportSettings& settings, const EncoderEnvironment& environment) -> ResolvedEncoder {
    const auto use_nvenc = settings.encoder_kind == EncoderKind::HevcNvenc
        || (settings.encoder_kind == EncoderKind::Auto
            && settings.auto_detect_gpu
            && environment.has_nvidia_gpu
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

auto output_path_for(
    const VideoMetadata& metadata,
    const ChapterSegment& chapter,
    const u16 chapter_index,
    const std::filesystem::path& output_directory,
    const ExportSettings& settings
) -> std::filesystem::path {
    auto chapter_name = settings.sanitize_file_names ? sanitize_file_component(chapter.name) : trim_copy(chapter.name);
    auto file_name = replace_all_copy(settings.naming_pattern, "%name%", chapter_name);
    file_name = replace_all_copy(file_name, "%source%", metadata.source_path.stem().string());
    file_name = replace_all_copy(file_name, "%index%", zero_padded_index(static_cast<u16>(chapter_index + 1), settings.index_padding));
    file_name = sanitize_file_component(file_name);

    return output_directory / (file_name + output_extension_for(metadata, settings));
}

auto build_ffmpeg_command(
    const VideoMetadata& metadata,
    const ChapterSegment& chapter,
    const std::filesystem::path& output_path,
    const ExportSettings& settings,
    const EncoderEnvironment& environment
) -> std::vector<std::string> {
    auto command = std::vector<std::string> {
        settings.ffmpeg_path,
    };

    if (settings.overwrite_mode == OverwriteMode::Overwrite) {
        command.emplace_back("-y");
    } else {
        command.emplace_back("-n");
    }

    const auto start_time = format_millisecond_timecode(chapter.start_ms);
    const auto duration_ms = chapter.end_ms - chapter.start_ms;
    const auto duration = format_millisecond_timecode(duration_ms);

    if (settings.seek_mode == SeekMode::Fast) {
        command.emplace_back("-ss");
        command.emplace_back(start_time);
    }

    command.emplace_back("-i");
    command.emplace_back(metadata.source_path.string());

    if (settings.seek_mode == SeekMode::Accurate) {
        command.emplace_back("-ss");
        command.emplace_back(start_time);
    }

    command.emplace_back("-t");
    command.emplace_back(duration);

    const auto resolved_encoder = resolve_encoder(settings, environment);
    command.emplace_back("-c:v");
    command.emplace_back(resolved_encoder.video_codec);
    command.insert(command.end(), resolved_encoder.arguments.begin(), resolved_encoder.arguments.end());

    if (settings.ffmpeg_threads > 0) {
        command.emplace_back("-threads");
        command.emplace_back(std::to_string(settings.ffmpeg_threads));
    }

    append_audio_arguments(command, settings);

    if (settings.copy_source_metadata) {
        command.emplace_back("-map_metadata");
        command.emplace_back("0");
    } else {
        command.emplace_back("-map_metadata");
        command.emplace_back("-1");
    }

    command.emplace_back("-metadata");
    command.emplace_back("title=" + trim_copy(chapter.name));

    if (output_path.extension() == ".mp4") {
        command.emplace_back("-movflags");
        command.emplace_back("+faststart");
    }

    const auto extra_arguments = split_quoted_arguments(settings.extra_ffmpeg_args);
    command.insert(command.end(), extra_arguments.begin(), extra_arguments.end());
    command.emplace_back(output_path.string());

    return command;
}

} // namespace vidchopper
