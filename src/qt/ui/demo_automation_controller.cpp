#include "qt/ui/demo_automation_controller.hpp"

#include "core/chapter_plan.hpp"
#include "core/ready_marker.hpp"

#include <QCoreApplication>
#include <QTimer>

#include <array>
#include <utility>

namespace vidchopper {

namespace {

constexpr auto demo_chapter_names = std::array {
    "Intro",
    "Setup Overview",
    "Key Features",
    "Demo",
    "Tips & Tricks",
    "Outro",
};

} // namespace

DemoAutomationController::DemoAutomationController(DemoLaunchOptions options, QObject* parent)
    : QObject(parent)
    , options_(std::move(options)) {
}

auto DemoAutomationController::enabled() const noexcept -> bool {
    return options_.enabled();
}

auto DemoAutomationController::window_size() const noexcept -> const std::optional<DemoWindowSize>& {
    return options_.window_size;
}

auto DemoAutomationController::capture_output_directory() const -> Path {
    return options_.demo_source.parent_path() / "captures";
}

auto DemoAutomationController::seeded_chapters(const u64 duration_ms) const -> std::vector<ChapterSegment> {
    auto chapters = build_default_chapters(duration_ms, static_cast<u8>(demo_chapter_names.size()));
    for (auto index = size_t {0}; index < chapters.size() && index < demo_chapter_names.size(); ++index) {
        chapters[index].name = demo_chapter_names[index];
    }
    return chapters;
}

auto DemoAutomationController::activate(LoadVideo load_video, SeedScene seed_scene) -> void {
    if (applied_ || !enabled()) {
        return;
    }
    applied_ = true;

    const DemoScene scene = options_.scene;
    const bool started = load_video(options_.demo_source,
        [this, scene, seed_scene = std::move(seed_scene)](const bool loaded) {
            finish(loaded && seed_scene(scene));
        });
    if (!started) {
        finish(false);
    }
}

auto DemoAutomationController::finish(const bool success) -> void {
    const auto status = success ? QStringLiteral("ready") : QStringLiteral("error");
    QTimer::singleShot(0, this, [this, status]() {
        if (!write_ready_file(status)) {
            QCoreApplication::exit(2);
        }
    });
}

auto DemoAutomationController::write_ready_file(const QString& status) -> bool {
    if (options_.demo_ready_file.empty()) {
        return true;
    }

    const auto result = write_ready_marker(options_.demo_ready_file, status.toStdString());
    if (result.ok()) {
        return true;
    }

    emit automation_error(QString::fromStdString(result.error_message));
    return false;
}

} // namespace vidchopper
