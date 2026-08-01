#include "qt/services/probe_coordinator.hpp"
#include "test_support.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>

using namespace vidchopper;

namespace {

[[nodiscard]] auto fixture_text() -> std::string {
    const Path fixture = Path {__FILE__}.parent_path() / "dummy" / "mock_data" / "mock_ffprobe_embedded_chapters.json";
    auto stream = std::ifstream {fixture};
    auto output = std::ostringstream {};
    output << stream.rdbuf();
    return output.str();
}

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
    const auto executable = Path {R"(C:\Program Files\ffmpeg\ffprobe.exe)"};
    const auto source = Path {R"(C:\Temp\match clips\set.mkv)"};
    const auto process = ProcessResult {
        .state = ProcessExitState::Success,
        .standard_output = fixture_text(),
    };
    const ProbeResult expected = parse_probe_output(executable, source, process);

    auto worker_started = std::atomic_bool {false};
    const auto delayed = [&worker_started, process](const ProcessRequest& request) -> ProcessResult {
        worker_started = true;
        for (auto index = 0; index < 50; ++index) {
            if (request.stop_token.stop_requested()) {
                return ProcessResult {.state = ProcessExitState::Cancelled};
            }
            QThread::msleep(2);
        }
        return process;
    };

    auto coordinator = ProbeCoordinator {delayed};
    auto finished = QSignalSpy {&coordinator, &ProbeCoordinator::finished};
    auto heartbeat = false;
    auto delivered_on_owner_thread = false;
    QObject::connect(&coordinator, &ProbeCoordinator::finished, &application, [&](const ProbeResult&) {
        delivered_on_owner_thread = QThread::currentThread() == application.thread();
    });

    test_support::expect_true(coordinator.start_probe(executable, source), "an idle coordinator should start probing");
    test_support::expect_true(coordinator.busy(), "a started coordinator should report busy");
    QTimer::singleShot(0, &application, [&heartbeat]() { heartbeat = true; });
    test_support::expect_true(finished.wait(2000), "queued probing should complete");
    test_support::expect_true(heartbeat, "the owner event loop should remain responsive during probing");
    test_support::expect_true(worker_started, "probing should run on the worker thread");
    test_support::expect_true(delivered_on_owner_thread, "probe completion should be delivered on the owner thread");
    const ProbeResult actual = finished.takeFirst().front().value<ProbeResult>();
    test_support::expect_true(actual.ok(), "the queued adapter should preserve a successful probe");
    test_support::expect_eq(
        actual.metadata, expected.metadata, "the Qt adapter and shared parser should return identical metadata");
    test_support::expect_true(!coordinator.busy(), "completion should return the coordinator to idle");

    auto cancellation_observed = std::atomic_bool {false};
    auto executor_finished = std::atomic_bool {false};
    auto lifetime_worker_started = std::atomic_bool {false};
    const auto cancellable = [&cancellation_observed, &executor_finished, &lifetime_worker_started](
                                 const ProcessRequest& request) -> ProcessResult {
        lifetime_worker_started = true;
        while (!request.stop_token.stop_requested()) {
            QThread::msleep(1);
        }
        cancellation_observed = true;
        executor_finished = true;
        return ProcessResult {.state = ProcessExitState::Cancelled};
    };
    auto* destructible = new ProbeCoordinator {cancellable};
    auto late_deliveries = 0;
    QObject::connect(
        destructible, &ProbeCoordinator::finished, &application, [&](const ProbeResult&) { ++late_deliveries; });
    test_support::expect_true(destructible->start_probe(executable, source), "lifetime test should start probing");
    test_support::expect_true(wait_until([&lifetime_worker_started]() { return lifetime_worker_started.load(); }, 2000),
        "lifetime test should enter its worker");
    delete destructible;
    test_support::expect_true(wait_until([&executor_finished]() { return executor_finished.load(); }, 2000),
        "destruction should cancel worker execution");
    test_support::expect_true(cancellation_observed, "destruction should propagate cancellation");
    QCoreApplication::processEvents();
    test_support::expect_eq(late_deliveries, 0, "a destroyed coordinator should not receive completion");

    static_cast<void>(application);
    return 0;
}
