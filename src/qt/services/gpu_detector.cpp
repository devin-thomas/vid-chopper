#include "qt/services/gpu_detector.hpp"

#include "qt/path_utils.hpp"
#include "services/encoder_capability.hpp"

#include <QMetaObject>
#include <QThread>

#include <chrono>
#include <mutex>
#include <stop_token>
#include <utility>

namespace vidchopper {

struct GpuDetector::TaskState {
    std::mutex receiver_mutex;
    GpuDetector* receiver {nullptr};
    std::stop_source stop_source;
};

namespace {

[[nodiscard]] auto capability_settings(const QString& ffmpeg_path, const EncoderKind backend) -> ExportSettings {
    auto settings = ExportSettings {};
    settings.ffmpeg_path = path_to_utf8(qstring_to_path(ffmpeg_path));
    settings.encoder_kind = backend;
    return settings;
}

[[nodiscard]] auto capability_diagnostic(const EncoderCapabilityResult& result) -> QString {
    const QString backend_name = utf8_to_qstring(result.backend_display_name);
    if (result.available()) {
        return QStringLiteral("%1 capability test passed.").arg(backend_name);
    }

    return QStringLiteral("%1 capability unavailable: %2").arg(backend_name, utf8_to_qstring(result.rejection_reason));
}

} // namespace

GpuDetector::GpuDetector(QObject* parent)
    : GpuDetector(run_process, parent) {
}

GpuDetector::GpuDetector(ProcessExecutor executor, QObject* parent)
    : QObject(parent)
    , executor_ {std::move(executor)} {
}

GpuDetector::~GpuDetector() {
    const std::shared_ptr<TaskState> state = std::move(state_);
    if (state == nullptr) {
        return;
    }

    state->stop_source.request_stop();
    const auto lock = std::scoped_lock {state->receiver_mutex};
    state->receiver = nullptr;
}

auto GpuDetector::busy() const noexcept -> bool {
    return state_ != nullptr;
}

auto GpuDetector::detect(const QString& ffmpeg_path) -> bool {
    if (busy()) {
        return false;
    }

    auto state = std::make_shared<TaskState>();
    state->receiver = this;
    state_ = state;

    const EncoderPlatform platform = current_encoder_platform();
    const EncoderKind backend = platform == EncoderPlatform::MacOs ? EncoderKind::HevcVideoToolbox
                                                                     : EncoderKind::HevcNvenc;
    const ExportSettings settings = capability_settings(ffmpeg_path, backend);
    ProcessExecutor executor = executor_;
    auto* thread = QThread::create([state, executor = std::move(executor), settings, backend, platform]() mutable {
        const auto options = EncoderCapabilityOptions {
            .timeout = std::chrono::seconds {3},
            .stdout_limit_bytes = 1024 * 1024,
            .stderr_limit_bytes = 4096,
            .stop_token = state->stop_source.get_token(),
            .use_encoder_listing_prefilter = true,
        };
        auto environment = EncoderEnvironment {.platform = platform};
        const auto result =
            EncoderCapabilityService {std::move(executor)}.test(settings, backend, environment, options);
        if (backend == EncoderKind::HevcVideoToolbox) {
            environment.has_hevc_videotoolbox_encoder = result.available();
        } else {
            environment.has_nvidia_gpu = result.available();
            environment.has_hevc_nvenc_encoder = result.available();
        }
        const QString diagnostic = capability_diagnostic(result);

        const auto lock = std::scoped_lock {state->receiver_mutex};
        if (state->receiver == nullptr) {
            return;
        }
        auto* receiver = state->receiver;
        static_cast<void>(QMetaObject::invokeMethod(
            receiver,
            [state, environment, diagnostic]() mutable {
                auto* active_receiver = static_cast<GpuDetector*>(nullptr);
                {
                    const auto state_lock = std::scoped_lock {state->receiver_mutex};
                    active_receiver = state->receiver;
                }
                if (active_receiver != nullptr) {
                    active_receiver->complete(state, environment, diagnostic);
                }
            },
            Qt::QueuedConnection));
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

auto GpuDetector::complete(
    const std::shared_ptr<TaskState>& state, EncoderEnvironment environment, QString diagnostic) -> void {
    if (state_ != state) {
        return;
    }

    {
        const auto lock = std::scoped_lock {state->receiver_mutex};
        state->receiver = nullptr;
    }
    state_.reset();
    emit finished(environment, diagnostic);
}

} // namespace vidchopper
