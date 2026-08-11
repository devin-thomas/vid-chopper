#include "qt/services/export_coordinator.hpp"

#include "services/export_engine.hpp"
#include "services/export_planner.hpp"
#include "services/manifest_writer.hpp"

#include <QDir>
#include <QMetaObject>
#include <QThread>

#include <functional>
#include <mutex>
#include <stop_token>
#include <string_view>
#include <utility>

namespace vidchopper {

struct ExportCoordinator::TaskState {
    std::mutex receiver_mutex;
    ExportCoordinator* receiver {nullptr};
    std::stop_source stop_source;
};

namespace {

[[nodiscard]] auto display_path(const std::filesystem::path& path) -> QString {
    return QDir::toNativeSeparators(QString::fromStdWString(path.wstring()));
}

[[nodiscard]] auto segment_error(const RenderedSegment& segment) -> std::string {
    if (!segment.verification_error.empty()) {
        return segment.verification_error;
    }
    if (!segment.process.error_message.empty()) {
        return segment.process.error_message;
    }
    return segment.process.standard_error;
}

} // namespace

ExportCoordinator::ExportCoordinator(QObject* parent)
    : ExportCoordinator(run_process, parent) {
}

ExportCoordinator::ExportCoordinator(ProcessExecutor executor, QObject* parent)
    : QObject(parent)
    , executor_ {std::move(executor)} {
}

ExportCoordinator::~ExportCoordinator() {
    cancel();
    const std::shared_ptr<TaskState> state = std::move(state_);
    if (state == nullptr) {
        return;
    }

    const auto lock = std::scoped_lock {state->receiver_mutex};
    state->receiver = nullptr;
}

auto ExportCoordinator::busy() const noexcept -> bool {
    return state_ != nullptr;
}

auto ExportCoordinator::start_export(const VideoMetadata& metadata,
    const std::vector<ChapterSegment>& chapters,
    const std::filesystem::path& output_directory,
    const ExportSettings& settings,
    const EncoderEnvironment& environment) -> void {
    if (busy()) {
        emit log_message(LogCategory::Warning, "Export is already running.");
        return;
    }

    OutputPlanResult plan = plan_outputs({OutputPlanInput {
        .metadata = metadata,
        .output_directory = output_directory,
        .chapters = chapters,
        .settings = settings,
        .environment = environment,
    }});
    if (!plan.ok()) {
        auto errors = QStringList {};
        for (const std::string& error : plan.errors) {
            errors.push_back(QString::fromStdString(error));
        }
        emit finished(false, errors);
        return;
    }

    auto state = std::make_shared<TaskState>();
    state->receiver = this;
    state_ = state;
    ProcessExecutor executor = executor_;
    auto jobs = std::move(plan.jobs);

    auto* thread = QThread::create([state, executor = std::move(executor), jobs = std::move(jobs)]() mutable {
        const auto dispatch = [state](std::function<void(ExportCoordinator&)> callback) {
            const auto lock = std::scoped_lock {state->receiver_mutex};
            if (state->receiver == nullptr) {
                return;
            }
            auto* receiver = state->receiver;
            static_cast<void>(QMetaObject::invokeMethod(
                receiver,
                [state, callback = std::move(callback)]() mutable {
                    auto* active_receiver = static_cast<ExportCoordinator*>(nullptr);
                    {
                        const auto state_lock = std::scoped_lock {state->receiver_mutex};
                        active_receiver = state->receiver;
                    }
                    if (active_receiver != nullptr) {
                        callback(*active_receiver);
                    }
                },
                Qt::QueuedConnection));
        };

        const auto options = ExportRunOptions {
            .stop_token = state->stop_source.get_token(),
            .progress_changed =
                [dispatch](const int percent) {
                    dispatch([percent](ExportCoordinator& receiver) { emit receiver.progress_changed(percent); });
                },
            .process_output =
                [dispatch](const std::string_view chunk) {
                    const QString output =
                        QString::fromUtf8(chunk.data(), static_cast<qsizetype>(chunk.size())).trimmed();
                    if (!output.isEmpty()) {
                        dispatch([output](ExportCoordinator& receiver) {
                            emit receiver.log_message(LogCategory::ProcessRaw, output);
                        });
                    }
                },
            .segment_started =
                [dispatch](const size_t,
                    const size_t,
                    const size_t chapter_index,
                    const size_t chapter_count,
                    const ResolvedExportJob&,
                    const PlannedExportSegment& segment) {
                    const QString output = display_path(segment.output_path);
                    dispatch([chapter_index, chapter_count, output](ExportCoordinator& receiver) {
                        emit receiver.chapter_started(
                            static_cast<int>(chapter_index), static_cast<int>(chapter_count), output);
                        emit receiver.log_message(
                            LogCategory::ExportLifecycle, QStringLiteral("Exporting %1").arg(output));
                    });
                },
            .segment_finished =
                [dispatch](const RenderedSegment& segment) {
                    if (!segment.ok()) {
                        const QString error = QString::fromStdString(segment_error(segment));
                        dispatch([error](ExportCoordinator& receiver) {
                            emit receiver.log_message(LogCategory::Error, error);
                        });
                    }
                },
            .message =
                [dispatch](const std::string& message) {
                    const QString text = QString::fromStdString(message);
                    dispatch([text](ExportCoordinator& receiver) {
                        emit receiver.log_message(LogCategory::ExportLifecycle, text);
                    });
                },
        };

        ExportRunResult run_result = ExportEngine {executor}.run(jobs, options);
        ManifestWriteResult manifest_result = write_manifests(jobs, run_result);
        dispatch([state, run_result = std::move(run_result), manifest_result = std::move(manifest_result)](
                     ExportCoordinator& receiver) mutable {
            receiver.complete(state, std::move(run_result), std::move(manifest_result));
        });
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    emit progress_changed(0);
    thread->start();
}

auto ExportCoordinator::cancel() -> void {
    if (state_ == nullptr) {
        return;
    }
    state_->stop_source.request_stop();
    emit log_message(LogCategory::ExportLifecycle, "Cancellation requested. Waiting for ffmpeg to stop.");
}

auto ExportCoordinator::complete(
    const std::shared_ptr<TaskState>& state, ExportRunResult run_result, ManifestWriteResult manifest_result) -> void {
    if (state_ != state) {
        return;
    }
    {
        const auto lock = std::scoped_lock {state->receiver_mutex};
        state->receiver = nullptr;
    }
    state_.reset();

    auto errors = QStringList {};
    for (const ExportJobResult& job : run_result.jobs) {
        if (!job.error_message.empty()) {
            errors.push_back(QString::fromStdString(job.error_message));
        }
        for (const RenderedSegment& segment : job.segments) {
            if (!segment.ok()) {
                errors.push_back(QString::fromStdString(segment_error(segment)));
            }
        }
    }
    for (const std::string& error : manifest_result.errors) {
        errors.push_back(QString::fromStdString(error));
    }

    emit finished(run_result.ok() && manifest_result.ok(), errors);
}

} // namespace vidchopper
