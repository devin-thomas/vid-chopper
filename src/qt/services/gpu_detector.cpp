#include "qt/services/gpu_detector.hpp"

#include <QProcess>
#include <QTimer>

namespace vidchopper {

namespace {

[[nodiscard]] auto powershell_arguments() -> QStringList {
    return {
        "-NoProfile",
        "-Command",
        "(Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name) -join \"`n\"",
    };
}

} // namespace

GpuDetector::GpuDetector(QObject* parent)
    : QObject(parent)
    , gpu_process_(new QProcess {this})
    , ffmpeg_process_(new QProcess {this})
    , gpu_timeout_(new QTimer {this})
    , ffmpeg_timeout_(new QTimer {this}) {
    gpu_timeout_->setSingleShot(true);
    ffmpeg_timeout_->setSingleShot(true);

    connect(gpu_process_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this](const int, const QProcess::ExitStatus) { finish_gpu_detection(); });
    connect(gpu_process_, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && !tried_windows_powershell_) {
            tried_windows_powershell_ = true;
            start_gpu_process("powershell");
            return;
        }
        if (error == QProcess::FailedToStart || error == QProcess::Crashed) {
            finish_gpu_detection();
        }
    });
    connect(gpu_timeout_, &QTimer::timeout, this, [this]() {
        if (gpu_process_->state() != QProcess::NotRunning) {
            gpu_process_->kill();
        }
        finish_gpu_detection();
    });

    connect(ffmpeg_process_,
        qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
        this,
        [this](const int, const QProcess::ExitStatus) { finish_ffmpeg_detection(); });
    connect(ffmpeg_process_, &QProcess::errorOccurred, this, [this](const QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart || error == QProcess::Crashed) {
            finish_ffmpeg_detection();
        }
    });
    connect(ffmpeg_timeout_, &QTimer::timeout, this, [this]() {
        if (ffmpeg_process_->state() != QProcess::NotRunning) {
            ffmpeg_process_->kill();
        }
        finish_ffmpeg_detection();
    });
}

auto GpuDetector::busy() const noexcept -> bool {
    return detecting_;
}

auto GpuDetector::detect(const QString& ffmpeg_path) -> bool {
    if (detecting_) {
        return false;
    }

    environment_ = {};
    detecting_ = true;
    gpu_complete_ = false;
    ffmpeg_complete_ = false;
    tried_windows_powershell_ = false;

    start_gpu_process("pwsh");
    ffmpeg_timeout_->start(4000);
    ffmpeg_process_->start(ffmpeg_path, {"-hide_banner", "-encoders"});
    return true;
}

auto GpuDetector::start_gpu_process(const QString& executable) -> void {
    gpu_timeout_->stop();
    gpu_timeout_->start(3000);
    gpu_process_->start(executable, powershell_arguments());
}

auto GpuDetector::finish_gpu_detection() -> void {
    if (gpu_complete_) {
        return;
    }

    gpu_timeout_->stop();
    const auto gpu_names = QString::fromUtf8(gpu_process_->readAllStandardOutput()).toLower();
    environment_.has_nvidia_gpu = gpu_names.contains("nvidia");
    gpu_complete_ = true;
    complete_if_ready();
}

auto GpuDetector::finish_ffmpeg_detection() -> void {
    if (ffmpeg_complete_) {
        return;
    }

    ffmpeg_timeout_->stop();
    const auto encoders = QString::fromUtf8(ffmpeg_process_->readAllStandardOutput()).toLower()
        + QString::fromUtf8(ffmpeg_process_->readAllStandardError()).toLower();
    environment_.has_hevc_nvenc_encoder = encoders.contains("hevc_nvenc");
    ffmpeg_complete_ = true;
    complete_if_ready();
}

auto GpuDetector::complete_if_ready() -> void {
    if (!gpu_complete_ || !ffmpeg_complete_) {
        return;
    }

    detecting_ = false;
    emit finished(environment_);
}

} // namespace vidchopper
