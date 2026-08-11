#pragma once

#include "core/types.hpp"
#include "services/process_runner.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace vidchopper {

enum class ToolKind : u8 {
    Ffmpeg = 0,
    Ffprobe = 1,
};

enum class ToolDiscoverySource : u8 {
    None = 0,
    ConfiguredPath = 1,
    Path = 2,
    Homebrew = 3,
    StandardLocation = 4,
};

struct ToolVersion {
    u32 major {0};
    u32 minor {0};
    u32 patch {0};

    [[nodiscard]] auto operator==(const ToolVersion&) const -> bool = default;
};

struct ToolCandidateDiagnostic {
    Path candidate;
    ToolDiscoverySource source {ToolDiscoverySource::None};
    bool exists {false};
    bool executable {false};
    std::string reason;
};

struct ToolResolution {
    ToolKind kind {ToolKind::Ffmpeg};
    bool success {false};
    Path selected_path;
    ToolVersion version;
    ToolDiscoverySource source {ToolDiscoverySource::None};
    std::vector<ToolCandidateDiagnostic> diagnostics;
    std::string failure_reason;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

struct ToolDiscoveryResult {
    bool success {false};
    ToolResolution ffmpeg {.kind = ToolKind::Ffmpeg};
    ToolResolution ffprobe {.kind = ToolKind::Ffprobe};
    std::vector<std::string> warnings;
    std::string failure_reason;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

struct ToolDiscoveryOptions {
    ProcessExecutor executor {run_process};
    std::optional<std::string> path_environment;
    std::vector<Path> additional_homebrew_paths;
    std::vector<Path> additional_standard_paths;
    bool use_platform_defaults {true};
    std::chrono::milliseconds version_timeout {5000};
};

class MediaToolResolver final {
public:
    explicit MediaToolResolver(ToolDiscoveryOptions options = {});

    [[nodiscard]] auto resolve(ToolKind kind, const Path& configured_path = {}) const -> ToolResolution;
    [[nodiscard]] auto resolve_pair(const Path& configured_ffmpeg, const Path& configured_ffprobe) const
        -> ToolDiscoveryResult;

private:
    ToolDiscoveryOptions options_;
};

[[nodiscard]] auto parse_tool_version(std::string_view output, ToolKind kind) -> std::optional<ToolVersion>;
[[nodiscard]] auto is_supported_tool_version(const ToolVersion& version) noexcept -> bool;
[[nodiscard]] auto format_tool_version(const ToolVersion& version) -> std::string;
[[nodiscard]] auto tool_kind_name(ToolKind kind) -> std::string_view;
[[nodiscard]] auto tool_discovery_source_name(ToolDiscoverySource source) -> std::string_view;

[[nodiscard]] auto discover_tool(
    ToolKind kind, const Path& configured_path = {}, const ToolDiscoveryOptions& options = {}) -> ToolResolution;
[[nodiscard]] auto discover_media_tools(
    const Path& configured_ffmpeg, const Path& configured_ffprobe, const ToolDiscoveryOptions& options = {})
    -> ToolDiscoveryResult;

} // namespace vidchopper
