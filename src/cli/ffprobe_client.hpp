#pragma once

#include "services/process_runner.hpp"
#include "core/models.hpp"

#include <string>

namespace vidchopper {

struct FfprobeResult {
    bool success {false};
    VideoMetadata metadata;
    ProcessResult process;
    std::string error_message;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

class FfprobeClient final {
public:
    explicit FfprobeClient(ProcessExecutor executor = run_process);

    [[nodiscard]] auto probe(const Path& executable, const Path& source_path) const -> FfprobeResult;

private:
    ProcessExecutor executor_;
};

[[nodiscard]] auto parse_ffprobe_output(
    const Path& executable, const Path& source_path, ProcessResult process) -> FfprobeResult;

} // namespace vidchopper
