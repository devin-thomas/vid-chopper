#pragma once

#include "core/models.h"

class QSettings;

namespace vidchopper {

[[nodiscard]] auto load_export_settings(QSettings& settings) -> ExportSettings;
auto save_export_settings(QSettings& settings, const ExportSettings& values) -> void;

} // namespace vidchopper
