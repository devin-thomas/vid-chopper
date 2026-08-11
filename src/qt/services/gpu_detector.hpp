#pragma once

#include "core/models.hpp"
#include "services/process_runner.hpp"

#include <QObject>
#include <QString>

#include <memory>

namespace vidchopper {

class GpuDetector final : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(GpuDetector)

public:
    explicit GpuDetector(QObject* parent = nullptr);
    explicit GpuDetector(ProcessExecutor executor, QObject* parent = nullptr);
    ~GpuDetector() override;

    [[nodiscard]] auto busy() const noexcept -> bool;
    [[nodiscard]] auto detect(const QString& ffmpeg_path) -> bool;

signals:
    void finished(const EncoderEnvironment& environment, const QString& diagnostic);

private:
    struct TaskState;

    auto complete(const std::shared_ptr<TaskState>& state, EncoderEnvironment environment, QString diagnostic) -> void;

    ProcessExecutor executor_;
    std::shared_ptr<TaskState> state_;
};

} // namespace vidchopper
