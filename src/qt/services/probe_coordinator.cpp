#include "qt/services/probe_coordinator.hpp"

#include <QMetaObject>
#include <QThread>

#include <mutex>
#include <stop_token>
#include <utility>

namespace vidchopper {

struct ProbeCoordinator::TaskState {
    std::mutex receiver_mutex;
    ProbeCoordinator* receiver {nullptr};
    std::stop_source stop_source;
};

ProbeCoordinator::ProbeCoordinator(QObject* parent)
    : ProbeCoordinator(run_process, parent) {
}

ProbeCoordinator::ProbeCoordinator(ProcessExecutor executor, QObject* parent)
    : QObject(parent)
    , executor_ {std::move(executor)} {
    qRegisterMetaType<ProbeResult>();
}

ProbeCoordinator::~ProbeCoordinator() {
    cancel();
    const std::shared_ptr<TaskState> state = std::move(state_);
    if (state == nullptr) {
        return;
    }

    const auto lock = std::scoped_lock {state->receiver_mutex};
    state->receiver = nullptr;
}

auto ProbeCoordinator::busy() const noexcept -> bool {
    return state_ != nullptr;
}

auto ProbeCoordinator::start_probe(const Path& executable, const Path& source_path) -> bool {
    if (busy()) {
        return false;
    }

    auto state = std::make_shared<TaskState>();
    state->receiver = this;
    state_ = state;
    ProcessExecutor executor = executor_;
    auto* thread = QThread::create([state, executor = std::move(executor), executable, source_path]() mutable {
        ProbeResult result =
            ProbeService {std::move(executor)}.probe(executable, source_path, state->stop_source.get_token());

        const auto lock = std::scoped_lock {state->receiver_mutex};
        if (state->receiver == nullptr) {
            return;
        }
        auto* receiver = state->receiver;
        static_cast<void>(QMetaObject::invokeMethod(
            receiver,
            [state, result = std::move(result)]() mutable {
                auto* active_receiver = static_cast<ProbeCoordinator*>(nullptr);
                {
                    const auto state_lock = std::scoped_lock {state->receiver_mutex};
                    active_receiver = state->receiver;
                }
                if (active_receiver != nullptr) {
                    active_receiver->complete(state, std::move(result));
                }
            },
            Qt::QueuedConnection));
    });
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

auto ProbeCoordinator::cancel() -> void {
    if (state_ != nullptr) {
        state_->stop_source.request_stop();
    }
}

auto ProbeCoordinator::complete(const std::shared_ptr<TaskState>& state, ProbeResult result) -> void {
    if (state_ != state) {
        return;
    }
    {
        const auto lock = std::scoped_lock {state->receiver_mutex};
        state->receiver = nullptr;
    }
    state_.reset();
    emit finished(result);
}

} // namespace vidchopper
