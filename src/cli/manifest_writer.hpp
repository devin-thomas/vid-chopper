#pragma once

#include "cli/export_runner.hpp"

#include <string>
#include <optional>
#include <vector>

namespace vidchopper {

struct ManifestWriteResult {
    bool success {false};
    std::vector<Path> written_paths;
    std::vector<std::string> errors;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

[[nodiscard]] auto write_manifests(const std::vector<ResolvedExportJob>& planned_jobs,
    const ExportRunResult& run_result,
    const std::optional<Path>& aggregate_json_path = std::nullopt,
    const std::optional<Path>& aggregate_csv_path = std::nullopt) -> ManifestWriteResult;

} // namespace vidchopper
