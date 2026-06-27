#pragma once

#include "core/models.h"

#include <QSize>
#include <QString>

class QObject;
class QSettings;

namespace vidchopper {

struct SettingsStore {
    QSettings* settings {nullptr};
    QString config_path;
};

[[nodiscard]] auto create_settings_store(QObject* parent) -> SettingsStore;
[[nodiscard]] auto load_export_settings(QSettings& settings) -> ExportSettings;
auto save_export_settings(QSettings& settings, const ExportSettings& values) -> void;
[[nodiscard]] auto load_zoom_percent(QSettings& settings) -> int;
auto save_zoom_percent(QSettings& settings, int zoom_percent) -> void;
[[nodiscard]] auto load_last_screen_size(QSettings& settings) -> QSize;
auto save_last_screen_size(QSettings& settings, const QSize& screen_size) -> void;
[[nodiscard]] auto clamp_zoom_percent(int zoom_percent) -> int;
[[nodiscard]] auto auto_zoom_percent_for_screen_height(int logical_height) -> int;

} // namespace vidchopper
