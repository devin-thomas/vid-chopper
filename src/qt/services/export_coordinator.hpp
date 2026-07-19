#pragma once

#include "core/models.hpp"
#include "qt/logging.hpp"

#include <QByteArray>
#include <QObject>
#include <QProcess>
#include <QStringList>

#include <cstddef>
#include <filesystem>
#include <vector>

namespace vidchopper {

class ExportCoordinator final : public QObject {
    Q_OBJECT

public:
    explicit ExportCoordinator(QObject* parent = nullptr);

    [[nodiscard]] auto busy() const -> bool;
    auto start_export(const VideoMetadata& metadata,
        std::vector<ChapterSegment> chapters,
        const std::filesystem::path& output_directory,
        const ExportSettings& settings,
        const EncoderEnvironment& environment) -> void;
    auto cancel() -> void;

signals:
    void log_message(LogCategory category, const QString& message);
    void progress_changed(int percent);
    void chapter_started(int current, int total, const QString& output_file);
    void finished(bool success, const QStringList& errors);

private slots:
    void handle_ready_read_stdout();
    void handle_ready_read_stderr();
    void handle_process_finished(int exit_code, QProcess::ExitStatus exit_status);
    void handle_process_error(QProcess::ProcessError error);

private:
    struct PendingExport {
        u16 chapter_index {0};
        ChapterSegment chapter;
        QString output_file;
        u64 duration_ms {0};
        QString program;
        QStringList arguments;
    };

    auto start_next() -> void;
    auto handle_failure(const QString& message) -> void;
    auto write_manifests() -> void;

    QProcess process_;
    QByteArray stdout_buffer_;
    std::vector<PendingExport> exports_;
    QStringList errors_;

    VideoMetadata metadata_;
    ExportSettings settings_;
    std::filesystem::path output_directory_;

    size_t current_index_ {0};
    u64 total_duration_ms_ {0};
    u64 completed_duration_ms_ {0};
    bool busy_ {false};
    bool cancel_requested_ {false};
};

} // namespace vidchopper
