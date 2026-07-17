#pragma once

#include <QMetaType>
#include <QString>

#include <vector>

namespace vidchopper {

enum class LogCategory {
    App,
    Config,
    Probe,
    ExportLifecycle,
    ExportProgress,
    ProcessRaw,
    Validation,
    Warning,
    Error,
};

struct LogEntry {
    LogCategory category {LogCategory::App};
    QString message;
};

} // namespace vidchopper

Q_DECLARE_METATYPE(vidchopper::LogCategory)
