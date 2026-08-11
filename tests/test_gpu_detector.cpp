#include "qt/services/gpu_detector.hpp"
#include "test_support.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <functional>

using namespace vidchopper;

namespace {

[[nodiscard]] auto wait_until(const std::function<bool()>& predicate, const int timeout_ms) -> bool {
    auto timer = QElapsedTimer {};
    timer.start();
    while (!predicate() && timer.elapsed() < timeout_ms) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
        QThread::msleep(1);
    }
    return predicate();
}

} // namespace

auto main(int argc, char* argv[]) -> int {
    auto application = QCoreApplication {argc, argv};
    auto executor_called = std::atomic_bool {false};
    auto executor_on_owner_thread = std::atomic_bool {false};
    const auto executor = [&application, &executor_called, &executor_on_owner_thread](
                              const ProcessRequest&) -> ProcessResult {
        executor_called = true;
        executor_on_owner_thread = QThread::currentThread() == application.thread();
        return ProcessResult {
            .state = ProcessExitState::FailedStart,
            .error_message = "capability fixture failed to start",
        };
    };

    auto detector = GpuDetector {executor};
    auto finished = false;
    auto delivered_on_owner_thread = false;
    auto diagnostic = QString {};
    QObject::connect(
        &detector, &GpuDetector::finished, &application, [&](const EncoderEnvironment&, const QString& message) {
            finished = true;
            delivered_on_owner_thread = QThread::currentThread() == application.thread();
            diagnostic = message;
        });

    auto heartbeat = false;
    QTimer::singleShot(0, &application, [&heartbeat]() { heartbeat = true; });
    test_support::expect_true(
        detector.detect(QStringLiteral("/tmp/ffmpeg with spaces")), "capability detection should start once");
    test_support::expect_true(wait_until([&finished]() { return finished; }, 2000),
        "capability detection should complete without blocking the event loop");
    test_support::expect_true(heartbeat, "the GUI event loop should remain responsive during capability detection");
    test_support::expect_true(!detector.busy(), "capability completion should return the detector to idle");
    test_support::expect_true(delivered_on_owner_thread, "capability completion should return to the owner thread");
    test_support::expect_true(!diagnostic.isEmpty(), "capability completion should retain an actionable diagnostic");
    if (executor_called) {
        test_support::expect_true(
            !executor_on_owner_thread, "the shared capability service should run off the GUI thread");
    }

    static_cast<void>(application);
    return 0;
}
