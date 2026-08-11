#pragma once

#include "core/models.hpp"
#include "services/process_runner.hpp"
#include "services/tool_discovery.hpp"

#include <stop_token>
#include <string>

namespace vidchopper {

inline constexpr auto maximum_probe_streams = size_t {256};

struct ProbeResult {
    bool success {false};
    VideoMetadata metadata;
    ProcessResult process;
    std::string error_message;
    ToolResolution tool;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

class ProbeService final {
public:
    explicit ProbeService(ProcessExecutor executor = run_process);

    [[nodiscard]] auto probe(
        const Path& executable, const Path& source_path, std::stop_token stop_token = {}) const -> ProbeResult;

private:
    ProcessExecutor executor_;
    MediaToolResolver tool_resolver_;
    bool validate_tools_ {false};
};

[[nodiscard]] auto parse_probe_output(
    const Path& executable, const Path& source_path, ProcessResult process) -> ProbeResult;

} // namespace vidchopper
