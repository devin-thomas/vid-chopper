#include "qt/app_settings.hpp"

#include "core/enum_utils.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#include <limits>

namespace vidchopper {

namespace {

template <typename Integer>
auto load_integer(QSettings& settings,
    const char* key,
    const Integer fallback,
    const qlonglong minimum,
    const qlonglong maximum,
    QStringList& diagnostics) -> Integer {
    bool converted = false;
    const qlonglong raw = settings.value(key, static_cast<qlonglong>(fallback)).toLongLong(&converted);
    if (!converted || raw < minimum || raw > maximum) {
        diagnostics.push_back(QStringLiteral("Invalid setting '%1'; using default %2 (expected %3-%4).")
                                  .arg(QString::fromLatin1(key))
                                  .arg(static_cast<qlonglong>(fallback))
                                  .arg(minimum)
                                  .arg(maximum));
        return fallback;
    }
    return static_cast<Integer>(raw);
}

template <typename Enum>
auto load_enum(
    QSettings& settings, const char* key, const Enum fallback, const Enum max_valid, QStringList& diagnostics) -> Enum {
    const auto raw =
        load_integer<int>(settings, key, static_cast<int>(fallback), 0, static_cast<int>(max_valid), diagnostics);
    return clamp_to_enum(raw, max_valid, fallback);
}

auto load_boolean(QSettings& settings, const char* key, const bool fallback, QStringList& diagnostics) -> bool {
    const auto value = settings.value(key, fallback).toString().trimmed().toLower();
    if (value == "true" || value == "1") {
        return true;
    }
    if (value == "false" || value == "0") {
        return false;
    }

    diagnostics.push_back(
        QStringLiteral("Invalid setting '%1'; using default %2 (expected true or false).")
            .arg(QString::fromLatin1(key), fallback ? QStringLiteral("true") : QStringLiteral("false")));
    return fallback;
}

auto settings_status_message(const QSettings& settings) -> QString {
    switch (settings.status()) {
    case QSettings::NoError:
        return {};
    case QSettings::AccessError:
        return QStringLiteral("Could not access settings file '%1'. Check the folder permissions.")
            .arg(QDir::toNativeSeparators(settings.fileName()));
    case QSettings::FormatError:
        return QStringLiteral("Settings file '%1' has an invalid format.")
            .arg(QDir::toNativeSeparators(settings.fileName()));
    }
    return QStringLiteral("Settings file '%1' reported an unknown error.")
        .arg(QDir::toNativeSeparators(settings.fileName()));
}

auto ensure_parent_directory(const QString& file_path) -> void {
    const auto parent = QFileInfo {file_path}.dir();
    if (!parent.exists()) {
        parent.mkpath(".");
    }
}

auto try_prepare_settings_file(const QString& file_path) -> bool {
    ensure_parent_directory(file_path);

    auto file = QFile {file_path};
    if (!file.exists()) {
        if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
            return false;
        }
        file.close();
        return true;
    }

    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        return false;
    }

    file.close();
    return true;
}

auto resolve_settings_path() -> QString {
    auto preferred_path = QDir {QCoreApplication::applicationDirPath()}.filePath("VidChopper.ini");
    if (try_prepare_settings_file(preferred_path)) {
        return preferred_path;
    }

    const auto fallback_directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    auto fallback_path = QDir {fallback_directory}.filePath("VidChopper.ini");
    if (try_prepare_settings_file(fallback_path)) {
        return fallback_path;
    }

    return preferred_path;
}

} // namespace

auto create_settings_store(QObject* parent) -> SettingsStore {
    const auto config_path = resolve_settings_path();
    return SettingsStore {
        .settings = new QSettings {config_path, QSettings::IniFormat, parent},
        .config_path = QDir::toNativeSeparators(config_path),
    };
}

auto load_app_settings(QSettings& settings) -> SettingsLoadResult {
    auto result = SettingsLoadResult {};
    auto& values = result.values.export_settings;

    values.ffmpeg_path =
        settings.value("tools/ffmpegPath", QString::fromStdString(values.ffmpeg_path)).toString().toStdString();
    values.ffprobe_path =
        settings.value("tools/ffprobePath", QString::fromStdString(values.ffprobe_path)).toString().toStdString();
    values.output_folder_pattern =
        settings.value("output/folderPattern", QString::fromStdString(values.output_folder_pattern))
            .toString()
            .toStdString();
    values.naming_pattern =
        settings.value("output/namingPattern", QString::fromStdString(values.naming_pattern)).toString().toStdString();
    values.x264_preset =
        settings.value("encoding/x264Preset", QString::fromStdString(values.x264_preset)).toString().toStdString();
    values.nvenc_preset =
        settings.value("encoding/nvencPreset", QString::fromStdString(values.nvenc_preset)).toString().toStdString();
    values.extra_ffmpeg_args = settings.value("tools/extraFfmpegArgs", QString::fromStdString(values.extra_ffmpeg_args))
                                   .toString()
                                   .toStdString();

    values.encoder_kind =
        load_enum(settings, "encoding/encoderKind", values.encoder_kind, EncoderKind::HevcNvenc, result.diagnostics);
    values.audio_mode =
        load_enum(settings, "encoding/audioMode", values.audio_mode, AudioMode::Aac, result.diagnostics);
    values.container_mode =
        load_enum(settings, "output/containerMode", values.container_mode, ContainerMode::Mkv, result.diagnostics);
    values.overwrite_mode =
        load_enum(settings, "output/overwriteMode", values.overwrite_mode, OverwriteMode::Skip, result.diagnostics);
    values.seek_mode = load_enum(settings, "precision/seekMode", values.seek_mode, SeekMode::Fast, result.diagnostics);
    values.display_mode = load_enum(
        settings, "precision/displayMode", values.display_mode, TimestampDisplayMode::Frames, result.diagnostics);

    values.default_chapter_count = load_integer<u8>(
        settings, "precision/defaultChapterCount", values.default_chapter_count, 1, 255, result.diagnostics);
    values.max_chapters =
        load_integer<u8>(settings, "precision/maxChapters", values.max_chapters, 1, 255, result.diagnostics);
    values.index_padding =
        load_integer<u8>(settings, "output/indexPadding", values.index_padding, 1, 6, result.diagnostics);
    values.x264_crf = load_integer<u8>(settings, "encoding/x264Crf", values.x264_crf, 0, 51, result.diagnostics);
    values.nvenc_cq = load_integer<u8>(settings, "encoding/nvencCq", values.nvenc_cq, 0, 51, result.diagnostics);
    values.min_chapter_seconds = load_integer<u8>(
        settings, "precision/minChapterSeconds", values.min_chapter_seconds, 1, 60, result.diagnostics);
    values.ffmpeg_threads =
        load_integer<u8>(settings, "encoding/ffmpegThreads", values.ffmpeg_threads, 0, 64, result.diagnostics);
    values.aac_bitrate_kbps =
        load_integer<u16>(settings, "encoding/aacBitrateKbps", values.aac_bitrate_kbps, 64, 512, result.diagnostics);

    values.auto_detect_gpu =
        load_boolean(settings, "encoding/autoDetectGpu", values.auto_detect_gpu, result.diagnostics);
    values.open_output_directory_after_export = load_boolean(
        settings, "output/openDirectoryAfterExport", values.open_output_directory_after_export, result.diagnostics);
    values.sanitize_file_names =
        load_boolean(settings, "output/sanitizeFileNames", values.sanitize_file_names, result.diagnostics);
    values.stop_on_first_error =
        load_boolean(settings, "execution/stopOnFirstError", values.stop_on_first_error, result.diagnostics);
    values.write_json_manifest =
        load_boolean(settings, "execution/writeJsonManifest", values.write_json_manifest, result.diagnostics);
    values.write_csv_manifest =
        load_boolean(settings, "execution/writeCsvManifest", values.write_csv_manifest, result.diagnostics);
    values.verify_output_durations =
        load_boolean(settings, "execution/verifyOutputDurations", values.verify_output_durations, result.diagnostics);
    values.copy_source_metadata =
        load_boolean(settings, "output/copySourceMetadata", values.copy_source_metadata, result.diagnostics);
    values.prefer_embedded_chapters =
        load_boolean(settings, "precision/preferEmbeddedChapters", values.prefer_embedded_chapters, result.diagnostics);
    values.confirm_remove_chapters = load_boolean(
        settings, "confirmations/confirmRemoveChapters", values.confirm_remove_chapters, result.diagnostics);
    values.confirm_exit = load_boolean(settings, "confirmations/confirmExit", values.confirm_exit, result.diagnostics);

    result.values.zoom_percent = ZoomPolicy::clamp(load_integer<int>(settings,
        "ui/zoomPercent",
        ZoomPolicy::default_percent,
        ZoomPolicy::minimum_percent,
        ZoomPolicy::maximum_percent,
        result.diagnostics));
    result.values.last_screen_size = QSize {
        load_integer<int>(settings, "ui/lastScreenWidth", 0, 0, std::numeric_limits<int>::max(), result.diagnostics),
        load_integer<int>(settings, "ui/lastScreenHeight", 0, 0, std::numeric_limits<int>::max(), result.diagnostics),
    };

    const auto status_message = settings_status_message(settings);
    if (!status_message.isEmpty()) {
        result.diagnostics.push_back(status_message + " Built-in defaults were used where necessary.");
    }
    return result;
}

auto save_app_settings(QSettings& settings, const AppSettingsSnapshot& snapshot) -> SettingsSaveResult {
    const auto& values = snapshot.export_settings;
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
    settings.setValue("confirmations/confirmRemoveChapters", values.confirm_remove_chapters);
    settings.setValue("confirmations/confirmExit", values.confirm_exit);

    settings.setValue("ui/zoomPercent", ZoomPolicy::clamp(snapshot.zoom_percent));
    settings.setValue("ui/lastScreenWidth", snapshot.last_screen_size.width());
    settings.setValue("ui/lastScreenHeight", snapshot.last_screen_size.height());
    settings.sync();

    const auto error_message = settings_status_message(settings);
    return SettingsSaveResult {
        .success = error_message.isEmpty(),
        .error_message = error_message,
    };
}

} // namespace vidchopper
