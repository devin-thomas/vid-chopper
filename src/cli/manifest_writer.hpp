#pragma once

#include "cli/export_runner.hpp"

#include <string>
#include <optional>
#include <vector>

namespace vidchopper {

struct ManifestJobWriteResult {
    size_t job_index {0};
    Path source_path;
    bool success {false};
    std::vector<Path> written_paths;
    std::vector<Path> preserved_media_paths;
    std::vector<std::string> errors;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

struct ManifestWriteResult {
    bool success {false};
    std::vector<Path> written_paths;
    std::vector<Path> preserved_media_paths;
    std::vector<std::string> errors;
    std::vector<ManifestJobWriteResult> jobs;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

[[nodiscard]] auto write_manifests(const std::vector<ResolvedExportJob>& planned_jobs,
    const ExportRunResult& run_result,
    const std::optional<Path>& aggregate_json_path = std::nullopt,
    const std::optional<Path>& aggregate_csv_path = std::nullopt) -> ManifestWriteResult;

} // namespace vidchopper
