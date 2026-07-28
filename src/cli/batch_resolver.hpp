#pragma once

#include "core/types.hpp"

#include <optional>
#include <string>
#include <vector>

namespace vidchopper {

struct BatchResolveRequest {
    Path source_path;
    std::optional<Path> chapter_source_path;
    bool use_embedded_chapters {false};
};

struct BatchJob {
    Path source_path;
    std::optional<Path> chapter_config_path;

    [[nodiscard]] auto operator==(const BatchJob&) const -> bool = default;
};

struct BatchResolution {
    std::vector<BatchJob> jobs;
    std::vector<std::string> errors;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

[[nodiscard]] auto resolve_batch(const BatchResolveRequest& request) -> BatchResolution;

} // namespace vidchopper
