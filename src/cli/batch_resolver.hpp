#pragma once

#include "core/types.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace vidchopper {

struct DirectoryScanFailure {
    std::optional<Path> entry_path;
    std::string message;
};

struct DirectoryScanResult {
    std::vector<Path> regular_files;
    std::vector<DirectoryScanFailure> failures;
    bool complete {true};
};

using DirectoryScanner = std::function<DirectoryScanResult(const Path&)>;

struct BatchResolveRequest {
    Path source_path;
    std::optional<Path> chapter_source_path;
    bool use_embedded_chapters {false};
    DirectoryScanner directory_scanner;
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
