#include "qt/app_settings.hpp"
#include "test_support.hpp"

#include <QFile>
#include <QSettings>
#include <QTemporaryDir>

using namespace vidchopper;

namespace {

auto contains_diagnostic(const QStringList& diagnostics, const QString& fragment) -> bool {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.contains(fragment)) {
            return true;
        }
    }
    return false;
}

} // namespace

auto main() -> int {
    test_support::expect_eq(ZoomPolicy::clamp(62), 50, "zoom should snap down to the nearest policy step");
    test_support::expect_eq(ZoomPolicy::clamp(63), 75, "zoom should snap up to the nearest policy step");
    test_support::expect_eq(ZoomPolicy::for_screen_height(1080), 100, "1080p should use the baseline zoom");
    test_support::expect_eq(ZoomPolicy::for_screen_height(0),
        ZoomPolicy::default_percent,
        "an unavailable screen height should use the default zoom");
    test_support::expect_eq(
        ZoomPolicy::presets().front(), ZoomPolicy::minimum_percent, "presets should start at the minimum zoom");
    test_support::expect_eq(
        ZoomPolicy::presets().back(), ZoomPolicy::maximum_percent, "presets should end at the maximum zoom");

    auto directory = QTemporaryDir {};
    test_support::expect_true(directory.isValid(), "temporary settings directory should be available");

    const auto valid_path = directory.filePath("valid.ini");
    auto valid_settings = QSettings {valid_path, QSettings::IniFormat};
    auto largest = AppSettingsSnapshot {};
    largest.export_settings.default_chapter_count = 255;
    largest.export_settings.max_chapters = 255;
    largest.export_settings.index_padding = 6;
    largest.export_settings.x264_crf = 51;
    largest.export_settings.nvenc_cq = 51;
    largest.export_settings.min_chapter_seconds = 60;
    largest.export_settings.ffmpeg_threads = 64;
    largest.export_settings.aac_bitrate_kbps = 512;
    largest.zoom_percent = 300;
    largest.last_screen_size = QSize {7680, 4320};

    const auto saved = save_app_settings(valid_settings, largest);
    test_support::expect_true(saved.success, "valid settings should be persisted successfully");

    const auto loaded_largest = load_app_settings(valid_settings);
    test_support::expect_true(loaded_largest.diagnostics.isEmpty(), "largest valid settings should not warn");
    test_support::expect_eq(loaded_largest.values.export_settings,
        largest.export_settings,
        "largest valid export settings should round-trip");
    test_support::expect_eq(loaded_largest.values.zoom_percent, 300, "largest valid zoom should round-trip");
    test_support::expect_eq(
        loaded_largest.values.last_screen_size, QSize {7680, 4320}, "largest valid screen size should round-trip");

    const auto invalid_path = directory.filePath("invalid-values.ini");
    auto invalid_settings = QSettings {invalid_path, QSettings::IniFormat};
    invalid_settings.setValue("encoding/encoderKind", "not-a-number");
    invalid_settings.setValue("precision/defaultChapterCount", 256);
    invalid_settings.setValue("precision/maxChapters", -1);
    invalid_settings.setValue("output/indexPadding", 0);
    invalid_settings.setValue("encoding/x264Crf", "broken");
    invalid_settings.setValue("encoding/nvencCq", 52);
    invalid_settings.setValue("precision/minChapterSeconds", 0);
    invalid_settings.setValue("encoding/ffmpegThreads", 65);
    invalid_settings.setValue("encoding/aacBitrateKbps", 65536);
    invalid_settings.setValue("execution/writeJsonManifest", "sometimes");
    invalid_settings.setValue("ui/zoomPercent", 301);
    invalid_settings.setValue("ui/lastScreenWidth", -1);
    invalid_settings.setValue("ui/lastScreenHeight", "huge");
    invalid_settings.sync();

    const auto defaults = ExportSettings {};
    const auto loaded_invalid = load_app_settings(invalid_settings);
    test_support::expect_eq(loaded_invalid.values.export_settings.encoder_kind,
        defaults.encoder_kind,
        "malformed enum should use its default");
    test_support::expect_eq(loaded_invalid.values.export_settings.default_chapter_count,
        defaults.default_chapter_count,
        "oversized chapter count should not narrow or wrap");
    test_support::expect_eq(loaded_invalid.values.export_settings.max_chapters,
        defaults.max_chapters,
        "negative chapter maximum should use its default");
    test_support::expect_eq(loaded_invalid.values.export_settings.aac_bitrate_kbps,
        defaults.aac_bitrate_kbps,
        "oversized bitrate should not narrow or wrap");
    test_support::expect_eq(loaded_invalid.values.export_settings.write_json_manifest,
        defaults.write_json_manifest,
        "malformed boolean should use its default");
    test_support::expect_eq(loaded_invalid.values.zoom_percent, 100, "oversized zoom should use its default");
    test_support::expect_eq(
        loaded_invalid.values.last_screen_size, QSize {0, 0}, "invalid screen dimensions should use defaults");
    test_support::expect_true(contains_diagnostic(loaded_invalid.diagnostics, "precision/defaultChapterCount"),
        "invalid numeric setting should identify its key");
    test_support::expect_true(contains_diagnostic(loaded_invalid.diagnostics, "encoding/x264Crf"),
        "failed numeric conversion should identify its key");
    test_support::expect_true(contains_diagnostic(loaded_invalid.diagnostics, "expected 64-512"),
        "invalid numeric setting should report its supported range");

    const auto malformed_path = directory.filePath("malformed.ini");
    auto malformed_file = QFile {malformed_path};
    test_support::expect_true(
        malformed_file.open(QIODevice::WriteOnly | QIODevice::Truncate), "malformed settings fixture should open");
    malformed_file.write("[broken\nvalue=1\n");
    malformed_file.close();
    auto malformed_settings = QSettings {malformed_path, QSettings::IniFormat};
    const auto malformed_load = load_app_settings(malformed_settings);
    test_support::expect_true(contains_diagnostic(malformed_load.diagnostics, "invalid format"),
        "format errors should produce an actionable load diagnostic");
    const auto malformed_save = save_app_settings(malformed_settings, AppSettingsSnapshot {});
    test_support::expect_true(!malformed_save.success, "format errors should not be reported as successful saves");

    auto inaccessible_settings = QSettings {directory.path(), QSettings::IniFormat};
    const auto inaccessible_save = save_app_settings(inaccessible_settings, AppSettingsSnapshot {});
    test_support::expect_true(!inaccessible_save.success, "access errors should not be reported as successful saves");
    test_support::expect_true(inaccessible_save.error_message.contains("permissions"),
        "access errors should include actionable permission guidance");

    return 0;
}
