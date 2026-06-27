#include "qt/app_settings.h"

#include "core/enum_utils.h"

#include <QSettings>

#include <array>
#include <concepts>
#include <cstddef>
#include <string_view>

namespace vidchopper {

namespace {

template <typename T>
struct SettingField {
    std::string_view key;
    T ExportSettings::*member;
};

// Single source of truth for every persisted key. load/save iterate the same
// tables, so a key can never drift between the two directions.
constexpr auto string_fields = std::to_array<SettingField<std::string>>({
    {"tools/ffmpegPath", &ExportSettings::ffmpeg_path},
    {"tools/ffprobePath", &ExportSettings::ffprobe_path},
    {"output/folderPattern", &ExportSettings::output_folder_pattern},
    {"output/namingPattern", &ExportSettings::naming_pattern},
    {"encoding/x264Preset", &ExportSettings::x264_preset},
    {"encoding/nvencPreset", &ExportSettings::nvenc_preset},
    {"tools/extraFfmpegArgs", &ExportSettings::extra_ffmpeg_args},
});

constexpr auto byte_fields = std::to_array<SettingField<u8>>({
    {"precision/defaultChapterCount", &ExportSettings::default_chapter_count},
    {"precision/maxChapters", &ExportSettings::max_chapters},
    {"output/indexPadding", &ExportSettings::index_padding},
    {"encoding/x264Crf", &ExportSettings::x264_crf},
    {"encoding/nvencCq", &ExportSettings::nvenc_cq},
    {"precision/minChapterSeconds", &ExportSettings::min_chapter_seconds},
    {"encoding/ffmpegThreads", &ExportSettings::ffmpeg_threads},
});

constexpr auto word_fields = std::to_array<SettingField<u16>>({
    {"encoding/aacBitrateKbps", &ExportSettings::aac_bitrate_kbps},
});

constexpr auto bool_fields = std::to_array<SettingField<bool>>({
    {"encoding/autoDetectGpu", &ExportSettings::auto_detect_gpu},
    {"output/openDirectoryAfterExport", &ExportSettings::open_output_directory_after_export},
    {"output/sanitizeFileNames", &ExportSettings::sanitize_file_names},
    {"execution/stopOnFirstError", &ExportSettings::stop_on_first_error},
    {"execution/writeJsonManifest", &ExportSettings::write_json_manifest},
    {"execution/writeCsvManifest", &ExportSettings::write_csv_manifest},
    {"execution/verifyOutputDurations", &ExportSettings::verify_output_durations},
    {"output/copySourceMetadata", &ExportSettings::copy_source_metadata},
    {"precision/preferEmbeddedChapters", &ExportSettings::prefer_embedded_chapters},
});

// Enum keys are shared between load and save the same way the field tables are;
// the clamp bounds differ per enum, so the calls stay explicit.
namespace enum_key {
constexpr auto encoder_kind = "encoding/encoderKind";
constexpr auto audio_mode = "encoding/audioMode";
constexpr auto container_mode = "output/containerMode";
constexpr auto overwrite_mode = "output/overwriteMode";
constexpr auto seek_mode = "precision/seekMode";
constexpr auto display_mode = "precision/displayMode";
} // namespace enum_key

auto to_qkey(std::string_view key) -> QString {
    return QString::fromUtf8(key.data(), static_cast<qsizetype>(key.size()));
}

template <std::unsigned_integral T, std::size_t N>
auto load_uint_fields(
    QSettings& settings, ExportSettings& values, const std::array<SettingField<T>, N>& fields) -> void {
    for (const auto& field : fields) {
        values.*(field.member) = static_cast<T>(settings.value(to_qkey(field.key), values.*(field.member)).toUInt());
    }
}

} // namespace

auto load_export_settings(QSettings& settings) -> ExportSettings {
    auto values = ExportSettings {};

    for (const auto& field : string_fields) {
        values.*(field.member) =
            settings.value(to_qkey(field.key), QString::fromStdString(values.*(field.member))).toString().toStdString();
    }

    values.encoder_kind =
        clamp_to_enum(settings.value(enum_key::encoder_kind, static_cast<int>(values.encoder_kind)).toInt(),
            EncoderKind::HevcNvenc,
            EncoderKind::Auto);
    values.audio_mode = clamp_to_enum(settings.value(enum_key::audio_mode, static_cast<int>(values.audio_mode)).toInt(),
        AudioMode::Aac,
        AudioMode::Copy);
    values.container_mode =
        clamp_to_enum(settings.value(enum_key::container_mode, static_cast<int>(values.container_mode)).toInt(),
            ContainerMode::Mkv,
            ContainerMode::Source);
    values.overwrite_mode =
        clamp_to_enum(settings.value(enum_key::overwrite_mode, static_cast<int>(values.overwrite_mode)).toInt(),
            OverwriteMode::Skip,
            OverwriteMode::Ask);
    values.seek_mode = clamp_to_enum(settings.value(enum_key::seek_mode, static_cast<int>(values.seek_mode)).toInt(),
        SeekMode::Fast,
        SeekMode::Accurate);
    values.display_mode =
        clamp_to_enum(settings.value(enum_key::display_mode, static_cast<int>(values.display_mode)).toInt(),
            TimestampDisplayMode::Frames,
            TimestampDisplayMode::Milliseconds);

    load_uint_fields(settings, values, byte_fields);
    load_uint_fields(settings, values, word_fields);

    for (const auto& field : bool_fields) {
        values.*(field.member) = settings.value(to_qkey(field.key), values.*(field.member)).toBool();
    }

    return values;
}

auto save_export_settings(QSettings& settings, const ExportSettings& values) -> void {
    for (const auto& field : string_fields) {
        settings.setValue(to_qkey(field.key), QString::fromStdString(values.*(field.member)));
    }

    settings.setValue(enum_key::encoder_kind, static_cast<int>(values.encoder_kind));
    settings.setValue(enum_key::audio_mode, static_cast<int>(values.audio_mode));
    settings.setValue(enum_key::container_mode, static_cast<int>(values.container_mode));
    settings.setValue(enum_key::overwrite_mode, static_cast<int>(values.overwrite_mode));
    settings.setValue(enum_key::seek_mode, static_cast<int>(values.seek_mode));
    settings.setValue(enum_key::display_mode, static_cast<int>(values.display_mode));

    for (const auto& field : byte_fields) {
        settings.setValue(to_qkey(field.key), values.*(field.member));
    }
    for (const auto& field : word_fields) {
        settings.setValue(to_qkey(field.key), values.*(field.member));
    }
    for (const auto& field : bool_fields) {
        settings.setValue(to_qkey(field.key), values.*(field.member));
    }
}

} // namespace vidchopper
