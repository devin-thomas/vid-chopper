#pragma once

#include "core/models.hpp"
#include "qt/logging.hpp"
#include "services/process_runner.hpp"

#include <QObject>
#include <QStringList>

#include <filesystem>
#include <memory>
#include <vector>

namespace vidchopper {

struct ExportRunResult;
struct ManifestWriteResult;

class ExportCoordinator final : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ExportCoordinator)

public:
    explicit ExportCoordinator(QObject* parent = nullptr);
    explicit ExportCoordinator(ProcessExecutor executor, QObject* parent = nullptr);
    ~ExportCoordinator() override;

    [[nodiscard]] auto busy() const noexcept -> bool;
    auto start_export(const VideoMetadata& metadata,
        const std::vector<ChapterSegment>& chapters,
        const std::filesystem::path& output_directory,
        const ExportSettings& settings,
        const EncoderEnvironment& environment) -> void;
    auto cancel() -> void;

signals:
    void log_message(LogCategory category, const QString& message);
    void progress_changed(int percent);
    void chapter_started(int current, int total, const QString& output_file);
    void finished(bool success, const QStringList& errors);

private:
    struct TaskState;

    auto complete(const std::shared_ptr<TaskState>& state,
        ExportRunResult run_result,
        ManifestWriteResult manifest_result) -> void;

    ProcessExecutor executor_;
    std::shared_ptr<TaskState> state_;
};

} // namespace vidchopper
