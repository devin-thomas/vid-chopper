#pragma once

#include "core/models.hpp"

#include <QObject>
#include <QString>

class QProcess;
class QTimer;

namespace vidchopper {

class GpuDetector final : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(GpuDetector)

public:
    explicit GpuDetector(QObject* parent = nullptr);

    [[nodiscard]] auto busy() const noexcept -> bool;
    [[nodiscard]] auto detect(const QString& ffmpeg_path) -> bool;

signals:
    void finished(const EncoderEnvironment& environment);

private:
    auto start_gpu_process(const QString& executable) -> void;
    auto finish_gpu_detection() -> void;
    auto finish_ffmpeg_detection() -> void;
    auto complete_if_ready() -> void;

    QProcess* gpu_process_ {nullptr};
    QProcess* ffmpeg_process_ {nullptr};
    QTimer* gpu_timeout_ {nullptr};
    QTimer* ffmpeg_timeout_ {nullptr};
    EncoderEnvironment environment_;
    bool detecting_ {false};
    bool gpu_complete_ {true};
    bool ffmpeg_complete_ {true};
    bool tried_windows_powershell_ {false};
};

} // namespace vidchopper
