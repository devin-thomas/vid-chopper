#pragma once

#include "cli/process_runner.hpp"
#include "core/models.hpp"

#include <chrono>
#include <string>
#include <vector>

namespace vidchopper {

enum class ExportExitCode : u8 {
    Success = 0,
    ExportFailure = 2,
    ToolingError = 3,
};

struct PlannedExportSegment {
    ChapterSegment chapter;
    u16 chapter_index {0};
    Path output_path;
    std::vector<std::string> command;

    [[nodiscard]] auto operator==(const PlannedExportSegment&) const -> bool = default;
};

struct ResolvedExportJob {
    VideoMetadata metadata;
    Path output_directory;
    ExportSettings settings;
    EncoderEnvironment environment;
    std::vector<PlannedExportSegment> segments;
};

struct RenderedSegment {
    Path source_path;
    u16 chapter_index {0};
    std::string chapter_name;
    Path output_path;
    ProcessResult process;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

struct ExportJobResult {
    Path source_path;
    std::vector<RenderedSegment> segments;
    std::string error_message;
    bool stopped_early {false};

    [[nodiscard]] auto ok() const noexcept -> bool;
};

struct ExportRunOptions {
    std::chrono::milliseconds process_timeout {std::chrono::hours {24}};
    size_t stdout_limit_bytes {1024 * 1024};
    size_t stderr_limit_bytes {4096};
};

struct ExportRunResult {
    std::vector<ExportJobResult> jobs;
    ExportExitCode exit_code {ExportExitCode::Success};
    bool stopped_early {false};

    [[nodiscard]] auto ok() const noexcept -> bool;
};

class ExportRunner final {
public:
    explicit ExportRunner(ProcessExecutor executor = run_process);

    [[nodiscard]] auto run(
        const std::vector<ResolvedExportJob>& jobs, const ExportRunOptions& options = {}) const -> ExportRunResult;

private:
    ProcessExecutor executor_;
};

} // namespace vidchopper
