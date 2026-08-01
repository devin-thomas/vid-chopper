#pragma once

#include "core/models.hpp"

#include <QSize>
#include <QString>
#include <QStringList>

class QObject;
class QSettings;

namespace vidchopper {

inline constexpr auto minimum_zoom_percent = 50;
inline constexpr auto maximum_zoom_percent = 300;
inline constexpr auto zoom_step_percent = 25;
inline constexpr auto default_zoom_percent = 100;

struct SettingsStore {
    QSettings* settings {nullptr};
    QString config_path;
};

struct AppSettingsSnapshot {
    ExportSettings export_settings;
    int zoom_percent {default_zoom_percent};
    QSize last_screen_size;
};

struct SettingsLoadResult {
    AppSettingsSnapshot values;
    QStringList diagnostics;
};

struct SettingsSaveResult {
    bool success {false};
    QString error_message;
};

[[nodiscard]] auto create_settings_store(QObject* parent) -> SettingsStore;
[[nodiscard]] auto load_app_settings(QSettings& settings) -> SettingsLoadResult;
[[nodiscard]] auto save_app_settings(QSettings& settings, const AppSettingsSnapshot& values) -> SettingsSaveResult;
[[nodiscard]] auto clamp_zoom_percent(int zoom_percent) -> int;
[[nodiscard]] auto auto_zoom_percent_for_screen_height(int logical_height) -> int;

} // namespace vidchopper
