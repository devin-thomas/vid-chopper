#pragma once

#include "services/probe_service.hpp"

#include <QMetaType>
#include <QObject>

#include <memory>

namespace vidchopper {

class ProbeCoordinator final : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ProbeCoordinator)

public:
    explicit ProbeCoordinator(QObject* parent = nullptr);
    explicit ProbeCoordinator(ProcessExecutor executor, QObject* parent = nullptr);
    ~ProbeCoordinator() override;

    [[nodiscard]] auto busy() const noexcept -> bool;
    [[nodiscard]] auto start_probe(const Path& executable, const Path& source_path) -> bool;
    auto cancel() -> void;

signals:
    void finished(const ProbeResult& result);

private:
    struct TaskState;

    auto complete(const std::shared_ptr<TaskState>& state, ProbeResult result) -> void;

    ProcessExecutor executor_;
    std::shared_ptr<TaskState> state_;
};

} // namespace vidchopper

Q_DECLARE_METATYPE(vidchopper::ProbeResult)
