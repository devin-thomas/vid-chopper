#pragma once

#include "core/models.hpp"
#include "qt/demo_launch_options.hpp"

#include <QObject>
#include <QString>

#include <functional>
#include <vector>

namespace vidchopper {

class DemoAutomationController final : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(DemoAutomationController)

public:
    using LoadVideo = std::function<bool(const Path&, std::function<void(bool)>)>;
    using SeedScene = std::function<bool(DemoScene)>;

    explicit DemoAutomationController(DemoLaunchOptions options, QObject* parent = nullptr);

    [[nodiscard]] auto enabled() const noexcept -> bool;
    [[nodiscard]] auto window_size() const noexcept -> const std::optional<DemoWindowSize>&;
    [[nodiscard]] auto capture_output_directory() const -> Path;
    [[nodiscard]] auto seeded_chapters(u64 duration_ms) const -> std::vector<ChapterSegment>;

    auto activate(LoadVideo load_video, SeedScene seed_scene) -> void;

signals:
    void automation_error(const QString& error_message);

private:
    auto finish(bool success) -> void;
    [[nodiscard]] auto write_ready_file(const QString& status) -> bool;

    DemoLaunchOptions options_;
    bool applied_ {false};
};

} // namespace vidchopper
