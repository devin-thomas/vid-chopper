#pragma once

#include "core/config_paths.hpp"
#include "core/models.hpp"
#include "qt/ui/zoom_policy.hpp"

#include <QSize>
#include <QString>
#include <QStringList>

class QObject;
class QSettings;

namespace vidchopper {

struct SettingsStore {
    QSettings* settings {nullptr};
    QString config_path;
    bool available {true};
    QString error_message;
};

struct AppSettingsSnapshot {
    ExportSettings export_settings;
    int zoom_percent {ZoomPolicy::default_percent};
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
} // namespace vidchopper
