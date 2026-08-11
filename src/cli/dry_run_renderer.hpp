#pragma once

#include "services/export_engine.hpp"

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

namespace vidchopper {

struct DryRunRenderResult {
    bool success {false};
    std::vector<std::string> errors;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

[[nodiscard]] auto render_dry_run(const std::vector<ResolvedExportJob>& jobs,
    std::ostream& output,
    std::ostream& error_output,
    const std::optional<Path>& aggregate_json_path = std::nullopt,
    const std::optional<Path>& aggregate_csv_path = std::nullopt) -> DryRunRenderResult;

} // namespace vidchopper
