#include "qt/services/export_coordinator.hpp"
#include "services/export_planner.hpp"
#include "test_support.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QSignalSpy>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

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

[[nodiscard]] auto metadata(const Path& source) -> VideoMetadata {
    return VideoMetadata {
        .source_path = source,
        .duration_ms = 2000,
        .frame_rate = FrameRate {.numerator = 30, .denominator = 1},
        .source_extension = ".mp4",
    };
}

[[nodiscard]] auto chapters() -> std::vector<ChapterSegment> {
    return {{.name = "Match", .start_ms = 0, .end_ms = 2000}};
}

} // namespace

auto main(int argc, char* argv[]) -> int {
    auto application = QCoreApplication {argc, argv};
    const Path root = std::filesystem::temp_directory_path() / "vidchopper-export-coordinator";
    auto cleanup_error = std::error_code {};
    std::filesystem::remove_all(root, cleanup_error);

    auto settings = ExportSettings {};
    settings.overwrite_mode = OverwriteMode::Overwrite;
    settings.verify_output_durations = false;
    const Path output_directory = root / "explicit output";
    const VideoMetadata source_metadata = metadata(root / "source.mp4");
    const std::vector<ChapterSegment> source_chapters = chapters();
    const OutputPlanResult expected_plan = plan_outputs({OutputPlanInput {
        .metadata = source_metadata,
        .output_directory = output_directory,
        .chapters = source_chapters,
        .settings = settings,
    }});
    test_support::expect_true(expected_plan.ok(), "coordinator parity fixture should plan");

    auto observed_requests = std::vector<ProcessRequest> {};
    auto worker_started = std::atomic_bool {false};
    const auto successful = [&observed_requests, &worker_started](const ProcessRequest& request) -> ProcessResult {
        worker_started = true;
        observed_requests.push_back(request);
        if (request.standard_output_chunk) {
            request.standard_output_chunk("out_time_us=1000000\nprogress=continue\n");
        }
        if (request.standard_error_chunk) {
            request.standard_error_chunk("bounded ffmpeg diagnostic");
        }
        return ProcessResult {.state = ProcessExitState::Success};
    };

    auto coordinator = ExportCoordinator {successful};
    auto finished = QSignalSpy {&coordinator, &ExportCoordinator::finished};
    auto progress = QSignalSpy {&coordinator, &ExportCoordinator::progress_changed};
    auto heartbeat = false;
    auto delivered_on_owner_thread = false;
    QObject::connect(&coordinator, &ExportCoordinator::finished, &application, [&](const bool, const QStringList&) {
        delivered_on_owner_thread = QThread::currentThread() == application.thread();
    });

    coordinator.start_export(source_metadata, source_chapters, output_directory, settings, EncoderEnvironment {});
    test_support::expect_true(coordinator.busy(), "a started export should report busy");
    QTimer::singleShot(0, &application, [&heartbeat]() { heartbeat = true; });
    test_support::expect_true(finished.wait(2000), "queued export should complete");
    test_support::expect_true(heartbeat, "the GUI event loop should remain responsive during export");
    test_support::expect_true(worker_started, "shared export should run on a worker thread");
    test_support::expect_true(delivered_on_owner_thread, "completion should be delivered on the GUI owner thread");
    test_support::expect_true(!coordinator.busy(), "completion should return the coordinator to idle");
    test_support::expect_eq(
        finished.takeFirst().front().toBool(), true, "successful shared export should stay successful");
    test_support::expect_eq(observed_requests.size(), size_t {1}, "one planned chapter should run one ffmpeg process");
    test_support::expect_eq(observed_requests.front().executable,
        Path {expected_plan.jobs.front().segments.front().command.front()},
        "GUI adapter should use the shared planned executable byte-for-byte");
    test_support::expect_eq(observed_requests.front().arguments,
        std::vector<std::string> {expected_plan.jobs.front().segments.front().command.begin() + 1,
            expected_plan.jobs.front().segments.front().command.end()},
        "GUI adapter should use the shared planned argument vector byte-for-byte");
    test_support::expect_true(progress.count() >= 3, "GUI adapter should receive start, streamed, and final progress");
    test_support::expect_true(std::filesystem::exists(output_directory / "vidchopper-manifest.json"),
        "GUI adapter should use the shared default JSON manifest writer");

    auto cancellation_observed = std::atomic_bool {false};
    auto cancellation_worker_started = std::atomic_bool {false};
    const auto cancellable = [&cancellation_observed, &cancellation_worker_started](
                                 const ProcessRequest& request) -> ProcessResult {
        cancellation_worker_started = true;
        while (!request.stop_token.stop_requested()) {
            QThread::msleep(1);
        }
        cancellation_observed = true;
        return ProcessResult {
            .state = ProcessExitState::Cancelled,
            .error_message = "cancelled by GUI request",
        };
    };
    auto cancelling_coordinator = ExportCoordinator {cancellable};
    auto cancelled_finished = QSignalSpy {&cancelling_coordinator, &ExportCoordinator::finished};
    cancelling_coordinator.start_export(
        source_metadata, source_chapters, root / "cancelled output", settings, EncoderEnvironment {});
    test_support::expect_true(
        wait_until([&cancellation_worker_started]() { return cancellation_worker_started.load(); }, 2000),
        "cancellation fixture should enter its process executor");
    cancelling_coordinator.cancel();
    test_support::expect_true(cancelled_finished.wait(2000), "cancellation should finish the GUI lifecycle");
    test_support::expect_true(cancellation_observed, "GUI cancellation should reach the shared process token");
    test_support::expect_eq(
        cancelled_finished.takeFirst().front().toBool(), false, "cancelled GUI export should fail explicitly");
    test_support::expect_true(!cancelling_coordinator.busy(), "cancelled export should return to idle");

    auto lifetime_worker_started = std::atomic_bool {false};
    auto lifetime_worker_finished = std::atomic_bool {false};
    const auto lifetime_executor = [&lifetime_worker_started, &lifetime_worker_finished](
                                       const ProcessRequest& request) -> ProcessResult {
        lifetime_worker_started = true;
        while (!request.stop_token.stop_requested()) {
            QThread::msleep(1);
        }
        lifetime_worker_finished = true;
        return ProcessResult {.state = ProcessExitState::Cancelled};
    };
    auto* destructible = new ExportCoordinator {lifetime_executor};
    auto late_deliveries = 0;
    QObject::connect(destructible, &ExportCoordinator::finished, &application, [&](const bool, const QStringList&) {
        ++late_deliveries;
    });
    destructible->start_export(
        source_metadata, source_chapters, root / "lifetime output", settings, EncoderEnvironment {});
    test_support::expect_true(wait_until([&lifetime_worker_started]() { return lifetime_worker_started.load(); }, 2000),
        "lifetime fixture should enter its worker");
    delete destructible;
    test_support::expect_true(
        wait_until([&lifetime_worker_finished]() { return lifetime_worker_finished.load(); }, 2000),
        "coordinator destruction should cancel shared execution");
    QCoreApplication::processEvents();
    test_support::expect_eq(late_deliveries, 0, "destroyed coordinator should not receive late completion");

    std::filesystem::remove_all(root, cleanup_error);
    static_cast<void>(application);
    return 0;
}
