#include "qt/app_settings.h"

#include <QSettings>

#include <type_traits>

namespace vidchopper {

namespace {

template <typename Enum>
auto clamp_enum(int raw, Enum max_valid, Enum fallback) -> Enum {
    if (raw < 0 || raw > static_cast<int>(max_valid)) {
        return fallback;
    }
    return static_cast<Enum>(raw);
}

} // namespace

auto load_export_settings(QSettings& settings) -> ExportSettings {
    auto values = ExportSettings {};

    values.ffmpeg_path = settings.value("tools/ffmpegPath", QString::fromStdString(values.ffmpeg_path)).toString().toStdString();
    values.ffprobe_path = settings.value("tools/ffprobePath", QString::fromStdString(values.ffprobe_path)).toString().toStdString();
    values.output_folder_pattern = settings.value("output/folderPattern", QString::fromStdString(values.output_folder_pattern)).toString().toStdString();
    values.naming_pattern = settings.value("output/namingPattern", QString::fromStdString(values.naming_pattern)).toString().toStdString();
    values.x264_preset = settings.value("encoding/x264Preset", QString::fromStdString(values.x264_preset)).toString().toStdString();
    values.nvenc_preset = settings.value("encoding/nvencPreset", QString::fromStdString(values.nvenc_preset)).toString().toStdString();
    values.extra_ffmpeg_args = settings.value("tools/extraFfmpegArgs", QString::fromStdString(values.extra_ffmpeg_args)).toString().toStdString();

    values.encoder_kind = clamp_enum(settings.value("encoding/encoderKind", static_cast<int>(values.encoder_kind)).toInt(), EncoderKind::HevcNvenc, EncoderKind::Auto);
    values.audio_mode = clamp_enum(settings.value("encoding/audioMode", static_cast<int>(values.audio_mode)).toInt(), AudioMode::Aac, AudioMode::Copy);
    values.container_mode = clamp_enum(settings.value("output/containerMode", static_cast<int>(values.container_mode)).toInt(), ContainerMode::Mkv, ContainerMode::Source);
    values.overwrite_mode = clamp_enum(settings.value("output/overwriteMode", static_cast<int>(values.overwrite_mode)).toInt(), OverwriteMode::Skip, OverwriteMode::Ask);
    values.seek_mode = clamp_enum(settings.value("precision/seekMode", static_cast<int>(values.seek_mode)).toInt(), SeekMode::Fast, SeekMode::Accurate);
    values.display_mode = clamp_enum(settings.value("precision/displayMode", static_cast<int>(values.display_mode)).toInt(), TimestampDisplayMode::Frames, TimestampDisplayMode::Milliseconds);

    values.default_chapter_count = static_cast<u8>(settings.value("precision/defaultChapterCount", values.default_chapter_count).toUInt());
    values.max_chapters = static_cast<u8>(settings.value("precision/maxChapters", values.max_chapters).toUInt());
    values.index_padding = static_cast<u8>(settings.value("output/indexPadding", values.index_padding).toUInt());
    values.x264_crf = static_cast<u8>(settings.value("encoding/x264Crf", values.x264_crf).toUInt());
    values.nvenc_cq = static_cast<u8>(settings.value("encoding/nvencCq", values.nvenc_cq).toUInt());
    values.min_chapter_seconds = static_cast<u8>(settings.value("precision/minChapterSeconds", values.min_chapter_seconds).toUInt());
    values.ffmpeg_threads = static_cast<u8>(settings.value("encoding/ffmpegThreads", values.ffmpeg_threads).toUInt());
    values.aac_bitrate_kbps = static_cast<u16>(settings.value("encoding/aacBitrateKbps", values.aac_bitrate_kbps).toUInt());

    values.auto_detect_gpu = settings.value("encoding/autoDetectGpu", values.auto_detect_gpu).toBool();
    values.open_output_directory_after_export = settings.value("output/openDirectoryAfterExport", values.open_output_directory_after_export).toBool();
    values.sanitize_file_names = settings.value("output/sanitizeFileNames", values.sanitize_file_names).toBool();
    values.stop_on_first_error = settings.value("execution/stopOnFirstError", values.stop_on_first_error).toBool();
    values.write_json_manifest = settings.value("execution/writeJsonManifest", values.write_json_manifest).toBool();
    values.write_csv_manifest = settings.value("execution/writeCsvManifest", values.write_csv_manifest).toBool();
    values.verify_output_durations = settings.value("execution/verifyOutputDurations", values.verify_output_durations).toBool();
    values.copy_source_metadata = settings.value("output/copySourceMetadata", values.copy_source_metadata).toBool();
    values.prefer_embedded_chapters = settings.value("precision/preferEmbeddedChapters", values.prefer_embedded_chapters).toBool();

    return values;
}

auto save_export_settings(QSettings& settings, const ExportSettings& values) -> void {
    settings.setValue("tools/ffmpegPath", QString::fromStdString(values.ffmpeg_path));
    settings.setValue("tools/ffprobePath", QString::fromStdString(values.ffprobe_path));
    settings.setValue("output/folderPattern", QString::fromStdString(values.output_folder_pattern));
    settings.setValue("output/namingPattern", QString::fromStdString(values.naming_pattern));
    settings.setValue("encoding/x264Preset", QString::fromStdString(values.x264_preset));
    settings.setValue("encoding/nvencPreset", QString::fromStdString(values.nvenc_preset));
    settings.setValue("tools/extraFfmpegArgs", QString::fromStdString(values.extra_ffmpeg_args));

    settings.setValue("encoding/encoderKind", static_cast<int>(values.encoder_kind));
    settings.setValue("encoding/audioMode", static_cast<int>(values.audio_mode));
    settings.setValue("output/containerMode", static_cast<int>(values.container_mode));
    settings.setValue("output/overwriteMode", static_cast<int>(values.overwrite_mode));
    settings.setValue("precision/seekMode", static_cast<int>(values.seek_mode));
    settings.setValue("precision/displayMode", static_cast<int>(values.display_mode));

    settings.setValue("precision/defaultChapterCount", values.default_chapter_count);
    settings.setValue("precision/maxChapters", values.max_chapters);
    settings.setValue("output/indexPadding", values.index_padding);
    settings.setValue("encoding/x264Crf", values.x264_crf);
    settings.setValue("encoding/nvencCq", values.nvenc_cq);
    settings.setValue("precision/minChapterSeconds", values.min_chapter_seconds);
    settings.setValue("encoding/ffmpegThreads", values.ffmpeg_threads);
    settings.setValue("encoding/aacBitrateKbps", values.aac_bitrate_kbps);

    settings.setValue("encoding/autoDetectGpu", values.auto_detect_gpu);
    settings.setValue("output/openDirectoryAfterExport", values.open_output_directory_after_export);
    settings.setValue("output/sanitizeFileNames", values.sanitize_file_names);
    settings.setValue("execution/stopOnFirstError", values.stop_on_first_error);
    settings.setValue("execution/writeJsonManifest", values.write_json_manifest);
    settings.setValue("execution/writeCsvManifest", values.write_csv_manifest);
    settings.setValue("execution/verifyOutputDurations", values.verify_output_durations);
    settings.setValue("output/copySourceMetadata", values.copy_source_metadata);
    settings.setValue("precision/preferEmbeddedChapters", values.prefer_embedded_chapters);
}

} // namespace vidchopper
