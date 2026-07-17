#include "qt/services/gpu_detector.hpp"

#include <QProcess>

namespace vidchopper {

auto GpuDetector::detect(const QString& ffmpeg_path) -> EncoderEnvironment {
    auto environment = EncoderEnvironment {};

    const auto ps_args = QStringList {
        "-NoProfile",
        "-Command",
        "(Get-CimInstance Win32_VideoController | Select-Object -ExpandProperty Name) -join \"`n\"",
    };

    auto gpu_process = QProcess {};
    gpu_process.start("pwsh", ps_args);
    if (!gpu_process.waitForStarted(1000)) {
        gpu_process.start("powershell", ps_args);
    }

    if (gpu_process.waitForFinished(3000)) {
        const auto gpu_names = QString::fromLocal8Bit(gpu_process.readAllStandardOutput()).toLower();
        environment.has_nvidia_gpu = gpu_names.contains("nvidia");
    }

    auto ffmpeg_process = QProcess {};
    ffmpeg_process.start(ffmpeg_path, {"-hide_banner", "-encoders"});
    if (ffmpeg_process.waitForFinished(4000)) {
        const auto encoders = QString::fromLocal8Bit(ffmpeg_process.readAllStandardOutput()).toLower()
            + QString::fromLocal8Bit(ffmpeg_process.readAllStandardError()).toLower();
        environment.has_hevc_nvenc_encoder = encoders.contains("hevc_nvenc");
    }

    return environment;
}

} // namespace vidchopper
