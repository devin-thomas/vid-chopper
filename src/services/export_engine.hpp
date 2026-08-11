#pragma once

#include "core/models.hpp"
#include "services/process_runner.hpp"

#include <chrono>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
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
    std::optional<Path> chapter_source_path;
    bool uses_embedded_chapters {false};
    Path output_directory;
    ExportSettings settings;
    EncoderEnvironment environment;
    std::vector<PlannedExportSegment> segments;

    [[nodiscard]] auto operator==(const ResolvedExportJob&) const -> bool = default;
};

struct RenderedSegment {
    Path source_path;
    u16 chapter_index {0};
    std::string chapter_name;
    Path output_path;
    ProcessResult process;
    bool duration_verified {false};
    u64 actual_duration_ms {0};
    std::string verification_error;
    bool skipped {false};
    bool overwrote_existing {false};

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
    std::stop_token stop_token;
    std::function<void(int)> progress_changed;
    std::function<void(std::string_view)> process_output;
    std::function<void(size_t, size_t, size_t, size_t, const ResolvedExportJob&, const PlannedExportSegment&)>
        segment_started;
    std::function<void(const RenderedSegment&)> segment_finished;
    std::function<void(const std::string&)> message;
};

struct ExportRunResult {
    std::vector<ExportJobResult> jobs;
    ExportExitCode exit_code {ExportExitCode::Success};
    bool stopped_early {false};
    bool cancelled {false};

    [[nodiscard]] auto ok() const noexcept -> bool;
};

class ExportEngine final {
public:
    explicit ExportEngine(ProcessExecutor executor = run_process);

    [[nodiscard]] auto run(
        const std::vector<ResolvedExportJob>& jobs, const ExportRunOptions& options = {}) const -> ExportRunResult;

private:
    ProcessExecutor executor_;
};

} // namespace vidchopper
