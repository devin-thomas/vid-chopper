#pragma once

#include "core/models.hpp"
#include "services/process_runner.hpp"

#include <QObject>
#include <QString>

#include <functional>
#include <optional>

namespace vidchopper {

class ProbeCoordinator;
struct ProbeResult;

class SessionController final : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SessionController)

public:
    explicit SessionController(QObject* parent = nullptr);
    explicit SessionController(ProcessExecutor executor, QObject* parent = nullptr);

    [[nodiscard]] auto probe_busy() const noexcept -> bool;
    [[nodiscard]] auto metadata() const noexcept -> const std::optional<VideoMetadata>&;
    [[nodiscard]] auto output_directory() const noexcept -> const Path&;
    [[nodiscard]] auto output_directory_overridden() const noexcept -> bool;

    [[nodiscard]] auto request_video_load(
        const Path& ffprobe, const Path& source, std::function<void(bool)> completion = {}) -> bool;
    auto set_output_directory(const Path& path, bool overridden) -> void;
    [[nodiscard]] auto reset_output_directory(const ExportSettings& settings) -> bool;

signals:
    void video_loaded();
    void video_load_failed(const QString& error_message);

private:
    auto handle_probe_finished(const ProbeResult& result) -> void;

    ProbeCoordinator* probe_coordinator_ {nullptr};
    std::optional<VideoMetadata> metadata_;
    Path output_directory_;
    std::function<void(bool)> completion_;
    bool output_directory_overridden_ {false};
};

} // namespace vidchopper
