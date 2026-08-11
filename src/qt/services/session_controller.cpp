#include "qt/services/session_controller.hpp"

#include "core/chapter_plan.hpp"
#include "qt/path_utils.hpp"
#include "qt/services/probe_coordinator.hpp"

#include <system_error>
#include <utility>

namespace vidchopper {

namespace {

[[nodiscard]] auto normalize_path_for_storage(const Path& path) -> Path {
    auto error = std::error_code {};
    auto canonical = std::filesystem::weakly_canonical(path, error);
    if (!error) {
        return canonical;
    }

    error.clear();
    return std::filesystem::absolute(path, error).lexically_normal();
}

} // namespace

SessionController::SessionController(QObject* parent)
    : SessionController(run_process, parent) {
}

SessionController::SessionController(ProcessExecutor executor, QObject* parent)
    : QObject(parent)
    , probe_coordinator_(new ProbeCoordinator {std::move(executor), this}) {
    connect(probe_coordinator_, &ProbeCoordinator::finished, this, &SessionController::handle_probe_finished);
}

auto SessionController::probe_busy() const noexcept -> bool {
    return probe_coordinator_->busy();
}

auto SessionController::metadata() const noexcept -> const std::optional<VideoMetadata>& {
    return metadata_;
}

auto SessionController::output_directory() const noexcept -> const Path& {
    return output_directory_;
}

auto SessionController::output_directory_overridden() const noexcept -> bool {
    return output_directory_overridden_;
}

auto SessionController::request_video_load(
    const Path& ffprobe, const Path& source, std::function<void(bool)> completion) -> bool {
    if (probe_coordinator_->busy()) {
        return false;
    }

    completion_ = std::move(completion);
    if (!probe_coordinator_->start_probe(ffprobe, normalize_path_for_storage(source))) {
        completion_ = {};
        return false;
    }
    return true;
}

auto SessionController::set_output_directory(const Path& path, const bool overridden) -> void {
    output_directory_ = normalize_path_for_storage(path);
    output_directory_overridden_ = overridden;
}

auto SessionController::reset_output_directory(const ExportSettings& settings) -> bool {
    if (!metadata_.has_value()) {
        return false;
    }

    set_output_directory(default_output_directory(metadata_->source_path, settings), false);
    return true;
}

auto SessionController::handle_probe_finished(const ProbeResult& result) -> void {
    std::function<void(bool)> completion = std::move(completion_);
    if (!result.ok()) {
        emit video_load_failed(utf8_to_qstring(result.error_message));
        if (completion) {
            completion(false);
        }
        return;
    }

    metadata_ = result.metadata;
    emit video_loaded();
    if (completion) {
        completion(true);
    }
}

} // namespace vidchopper
