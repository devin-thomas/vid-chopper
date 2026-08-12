#include "services/tool_discovery.hpp"

#include "core/path_utils.hpp"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace vidchopper {

namespace {

[[nodiscard]] auto path_to_text(const Path& path) -> std::string {
    return path_to_utf8(path);
}

[[nodiscard]] auto normalized_path(const Path& candidate) -> Path {
    auto error = std::error_code {};
    Path normalized = candidate;
    if (!normalized.is_absolute()) {
        normalized = std::filesystem::absolute(normalized, error);
        if (error) {
            error.clear();
            normalized = candidate;
        }
    }
    normalized = normalized.lexically_normal();
    const Path canonical = std::filesystem::weakly_canonical(normalized, error);
    return error ? normalized : canonical;
}

[[nodiscard]] auto path_key(const Path& path) -> std::string {
#ifdef _WIN32
    const std::u8string encoded = normalized_path(path).generic_u8string();
    auto key = std::string {};
    key.reserve(encoded.size());
    for (const char8_t character : encoded) {
        key.push_back(static_cast<char>(character));
    }
#else
    std::string key = normalized_path(path).generic_string();
#endif
#ifdef _WIN32
    std::ranges::transform(
        key, key.begin(), [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
#endif
    return key;
}

[[nodiscard]] auto is_executable(const Path& path) -> bool {
#ifdef _WIN32
    return _waccess(path.c_str(), 0) == 0;
#else
    return ::access(path.c_str(), X_OK) == 0;
#endif
}

[[nodiscard]] auto path_delimiter() noexcept -> char {
#ifdef _WIN32
    return ';';
#else
    return ':';
#endif
}

[[nodiscard]] auto path_entries(const std::string_view value) -> std::vector<Path> {
    auto entries = std::vector<Path> {};
    if (value.empty()) {
        return entries;
    }

    size_t begin = 0;
    while (begin <= value.size()) {
        const size_t end = value.find(path_delimiter(), begin);
        const size_t length = end == std::string_view::npos ? value.size() - begin : end - begin;
        entries.emplace_back(length == 0 ? Path {"."} : path_from_utf8(value.substr(begin, length)));
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    return entries;
}

[[nodiscard]] auto current_path_environment(const ToolDiscoveryOptions& options) -> std::string {
    if (options.path_environment.has_value()) {
        return *options.path_environment;
    }
#ifdef _WIN32
    size_t required_size = 0;
    if (_wgetenv_s(&required_size, nullptr, 0, L"PATH") != 0 || required_size == 0) {
        return {};
    }
    auto value = std::wstring(required_size, L'\0');
    size_t value_size = 0;
    if (_wgetenv_s(&value_size, value.data(), value.size(), L"PATH") != 0 || value_size == 0) {
        return {};
    }
    value.resize(std::wcslen(value.c_str()));
    return path_to_utf8(Path {std::move(value)});
#else
    const char* const environment = std::getenv("PATH");
    return environment == nullptr ? std::string {} : std::string {environment};
#endif
}

[[nodiscard]] auto executable_name(const ToolKind kind) -> Path {
    return Path {kind == ToolKind::Ffmpeg ? "ffmpeg" : "ffprobe"};
}

[[nodiscard]] auto has_path_component(const Path& path) -> bool {
    return path.is_absolute() || path.has_root_name() || path.has_parent_path();
}

struct Candidate {
    Path path;
    ToolDiscoverySource source {ToolDiscoverySource::None};
};

auto add_candidate(std::vector<Candidate>& candidates,
    std::vector<std::string>& seen,
    const Path& path,
    const ToolDiscoverySource source) -> void {
    if (path.empty()) {
        return;
    }
    const std::string key = path_key(path);
    if (std::ranges::find(seen, key) != seen.end()) {
        return;
    }
    seen.push_back(key);
    candidates.push_back(Candidate {.path = normalized_path(path), .source = source});
}

auto add_tool_candidate(std::vector<Candidate>& candidates,
    std::vector<std::string>& seen,
    const Path& path,
    const ToolDiscoverySource source) -> void {
#ifdef _WIN32
    if (path.extension().empty()) {
        auto executable_path = path;
        executable_path += ".exe";
        add_candidate(candidates, seen, executable_path, source);
        return;
    }
#endif
    add_candidate(candidates, seen, path, source);
}

[[nodiscard]] auto default_homebrew_paths() -> std::vector<Path> {
#ifdef _WIN32
    return {};
#elif defined(__APPLE__)
    // Finder-launched GUI processes often do not inherit the shell PATH. Keep the
    // Apple Silicon prefix first, then cover an unlinked formula and Intel Macs.
    return {Path {"/opt/homebrew/bin"},
        Path {"/opt/homebrew/opt/ffmpeg/bin"},
        Path {"/usr/local/bin"},
        Path {"/opt/local/bin"}};
#else
    return {Path {"/opt/homebrew/bin"}, Path {"/usr/local/bin"}, Path {"/home/linuxbrew/.linuxbrew/bin"}};
#endif
}

[[nodiscard]] auto default_standard_paths() -> std::vector<Path> {
#ifdef _WIN32
    return {};
#elif defined(__APPLE__)
    return {Path {"/usr/local/bin"}, Path {"/usr/bin"}, Path {"/bin"}, Path {"/opt/local/bin"}, Path {"/sw/bin"}};
#else
    return {Path {"/usr/local/bin"}, Path {"/usr/bin"}, Path {"/bin"}};
#endif
}

[[nodiscard]] auto candidates_for(const ToolKind kind,
    const Path& configured_path,
    const ToolDiscoveryOptions& options,
    bool& strict_configured_path) -> std::vector<Candidate> {
    auto candidates = std::vector<Candidate> {};
    auto seen = std::vector<std::string> {};
    const bool has_configured_path = !configured_path.empty();
    strict_configured_path = has_configured_path && has_path_component(configured_path);
    const Path name = has_configured_path ? configured_path.filename() : executable_name(kind);

    if (strict_configured_path) {
        add_tool_candidate(candidates, seen, configured_path, ToolDiscoverySource::ConfiguredPath);
    }

    if (!strict_configured_path) {
        for (const Path& directory : path_entries(current_path_environment(options))) {
            add_tool_candidate(candidates, seen, directory / name, ToolDiscoverySource::Path);
        }
        if (options.use_platform_defaults) {
            for (const Path& directory : default_homebrew_paths()) {
                add_tool_candidate(candidates, seen, directory / name, ToolDiscoverySource::Homebrew);
            }
        }
        for (const Path& directory : options.additional_homebrew_paths) {
            add_tool_candidate(candidates, seen, directory / name, ToolDiscoverySource::Homebrew);
        }
        if (options.use_platform_defaults) {
            for (const Path& directory : default_standard_paths()) {
                add_tool_candidate(candidates, seen, directory / name, ToolDiscoverySource::StandardLocation);
            }
        }
        for (const Path& directory : options.additional_standard_paths) {
            add_tool_candidate(candidates, seen, directory / name, ToolDiscoverySource::StandardLocation);
        }
    }
    return candidates;
}

[[nodiscard]] auto output_for_version(const ProcessResult& process) -> std::string {
    auto output = process.standard_output;
    if (!process.standard_error.empty()) {
        if (!output.empty()) {
            output.push_back('\n');
        }
        output += process.standard_error;
    }
    return output;
}

[[nodiscard]] auto platform_guidance() -> std::string_view {
#ifdef __APPLE__
    return "On macOS, install FFmpeg with Homebrew: brew install ffmpeg.";
#elif defined(__linux__)
    return "On Ubuntu, install FFmpeg with apt: sudo apt update && sudo apt install ffmpeg.";
#else
    return "Install FFmpeg and ensure both ffmpeg and ffprobe are available on PATH.";
#endif
}

[[nodiscard]] auto supported_range_text() -> std::string_view {
    return "Supported versions are 6.1 through 8.x.";
}

[[nodiscard]] auto parse_unsigned(std::string_view text, size_t& offset) -> std::optional<u32> {
    const char* const first = text.data() + offset;
    const char* const last = text.data() + text.size();
    u32 value = 0;
    const auto parsed = std::from_chars(first, last, value);
    if (parsed.ec != std::errc {} || parsed.ptr == first) {
        return std::nullopt;
    }
    offset += static_cast<size_t>(parsed.ptr - first);
    return value;
}

[[nodiscard]] auto tool_root(const Path& path) -> Path {
    const Path parent = path.parent_path();
    return parent.filename() == "bin" ? parent.parent_path() : parent;
}

[[nodiscard]] auto reject_candidate(ToolResolution& result,
    ToolCandidateDiagnostic diagnostic,
    std::string reason,
    const bool strict_configured_path) -> bool {
    diagnostic.reason = std::move(reason);
    result.diagnostics.push_back(std::move(diagnostic));
    if (!strict_configured_path) {
        return false;
    }

    result.failure_reason = std::format("{} '{}' failed: {}. {} {}",
        tool_kind_name(result.kind),
        path_to_text(result.diagnostics.back().candidate),
        result.diagnostics.back().reason,
        supported_range_text(),
        platform_guidance());
    return true;
}

auto set_no_tool_failure(ToolResolution& result) -> void {
    if (result.diagnostics.empty()) {
        result.failure_reason = std::format("Unable to find a supported {} executable: no candidates were checked. {} {}",
            tool_kind_name(result.kind),
            supported_range_text(),
            platform_guidance());
        return;
    }

    const ToolCandidateDiagnostic& last = result.diagnostics.back();
    result.failure_reason = std::format("Unable to find a supported {} executable after checking {} candidate(s). "
                                        "Last candidate '{}' ({}, {}). {} {}",
        tool_kind_name(result.kind),
        result.diagnostics.size(),
        path_to_text(last.candidate),
        tool_discovery_source_name(last.source),
        last.reason,
        supported_range_text(),
        platform_guidance());
}

} // namespace

auto ToolResolution::ok() const noexcept -> bool {
    return success;
}

auto ToolDiscoveryResult::ok() const noexcept -> bool {
    return success;
}

auto tool_kind_name(const ToolKind kind) -> std::string_view {
    return kind == ToolKind::Ffmpeg ? "ffmpeg" : "ffprobe";
}

auto tool_discovery_source_name(const ToolDiscoverySource source) -> std::string_view {
    switch (source) {
    case ToolDiscoverySource::None:
        return "none";
    case ToolDiscoverySource::ConfiguredPath:
        return "configured path";
    case ToolDiscoverySource::Path:
        return "PATH";
    case ToolDiscoverySource::Homebrew:
        return "Homebrew";
    case ToolDiscoverySource::StandardLocation:
        return "standard Unix location";
    }
    return "unknown";
}

auto parse_tool_version(const std::string_view output, const ToolKind kind) -> std::optional<ToolVersion> {
    auto lowered = std::string {output};
    std::ranges::transform(
        lowered, lowered.begin(), [](const unsigned char value) { return static_cast<char>(std::tolower(value)); });
    const std::string marker = std::format("{} version ", tool_kind_name(kind));
    const size_t marker_position = lowered.find(marker);
    if (marker_position == std::string::npos) {
        return std::nullopt;
    }

    size_t offset = marker_position + marker.size();
    if (offset < lowered.size() && lowered[offset] == 'n') {
        ++offset;
    }
    const std::optional<u32> major = parse_unsigned(lowered, offset);
    if (!major.has_value() || offset >= lowered.size() || lowered[offset] != '.') {
        return std::nullopt;
    }
    ++offset;
    const std::optional<u32> minor = parse_unsigned(lowered, offset);
    if (!minor.has_value()) {
        return std::nullopt;
    }

    auto patch = u32 {0};
    if (offset < lowered.size() && lowered[offset] == '.') {
        ++offset;
        const std::optional<u32> parsed_patch = parse_unsigned(lowered, offset);
        if (!parsed_patch.has_value()) {
            return std::nullopt;
        }
        patch = *parsed_patch;
    }
    return ToolVersion {.major = *major, .minor = *minor, .patch = patch};
}

auto is_supported_tool_version(const ToolVersion& version) noexcept -> bool {
    return (version.major == 6 && version.minor >= 1) || version.major == 7 || version.major == 8;
}

auto format_tool_version(const ToolVersion& version) -> std::string {
    return std::format("{}.{}.{}", version.major, version.minor, version.patch);
}

MediaToolResolver::MediaToolResolver(ToolDiscoveryOptions options)
    : options_ {std::move(options)} {
}

auto MediaToolResolver::resolve(const ToolKind kind, const Path& configured_path) const -> ToolResolution {
    auto result = ToolResolution {.kind = kind};
    bool strict_configured_path = false;
    const std::vector<Candidate> candidates = candidates_for(kind, configured_path, options_, strict_configured_path);
    for (const Candidate& candidate : candidates) {
        auto diagnostic = ToolCandidateDiagnostic {.candidate = candidate.path, .source = candidate.source};
        auto error = std::error_code {};
        diagnostic.exists = std::filesystem::exists(candidate.path, error) && !error;
        if (!diagnostic.exists) {
            if (reject_candidate(result,
                    std::move(diagnostic),
                    error ? error.message() : "does not exist",
                    strict_configured_path)) {
                return result;
            }
            continue;
        }

        diagnostic.executable =
            std::filesystem::is_regular_file(candidate.path, error) && !error && is_executable(candidate.path);
        if (!diagnostic.executable) {
            if (reject_candidate(result,
                    std::move(diagnostic),
                    error ? error.message() : "is not executable",
                    strict_configured_path)) {
                return result;
            }
            continue;
        }

        result.selected_path = candidate.path;
        if (!options_.executor) {
            static_cast<void>(reject_candidate(
                result, std::move(diagnostic), "no process executor is configured", true));
            return result;
        }
        const ProcessResult process = options_.executor(ProcessRequest {
            .executable = candidate.path,
            .arguments = {"-version"},
            .timeout = options_.version_timeout,
            .stdout_limit_bytes = 64 * 1024,
            .stderr_limit_bytes = 64 * 1024,
        });
        if (!process.ok()) {
            const std::string reason = std::format("-version returned {}{}",
                process_exit_state_name(process.state),
                process.error_message.empty() ? "" : ": " + process.error_message);
            if (reject_candidate(result, std::move(diagnostic), reason, strict_configured_path)) {
                return result;
            }
            continue;
        }

        const std::optional<ToolVersion> version = parse_tool_version(output_for_version(process), kind);
        if (!version.has_value()) {
            if (reject_candidate(result,
                    std::move(diagnostic),
                    "-version output has no parseable version",
                    strict_configured_path)) {
                return result;
            }
            continue;
        }
        result.version = *version;
        if (!is_supported_tool_version(result.version)) {
            const std::string reason = std::format("reports unsupported version {}", format_tool_version(result.version));
            if (reject_candidate(result, std::move(diagnostic), reason, strict_configured_path)) {
                return result;
            }
            continue;
        }

        diagnostic.reason = std::format("selected version {}", format_tool_version(result.version));
        result.diagnostics.push_back(std::move(diagnostic));
        result.success = true;
        result.source = candidate.source;
        return result;
    }

    set_no_tool_failure(result);
    return result;
}

auto MediaToolResolver::resolve_pair(
    const Path& configured_ffmpeg, const Path& configured_ffprobe) const -> ToolDiscoveryResult {
    auto result = ToolDiscoveryResult {};
    result.ffmpeg = resolve(ToolKind::Ffmpeg, configured_ffmpeg);
    result.ffprobe = resolve(ToolKind::Ffprobe, configured_ffprobe);
    if (!result.ffmpeg.ok() || !result.ffprobe.ok()) {
        if (!result.ffmpeg.ok()) {
            result.failure_reason += result.ffmpeg.failure_reason;
        }
        if (!result.ffprobe.ok()) {
            if (!result.failure_reason.empty()) {
                result.failure_reason += " ";
            }
            result.failure_reason += result.ffprobe.failure_reason;
        }
        return result;
    }

    const std::string ffmpeg_version = format_tool_version(result.ffmpeg.version);
    const std::string ffprobe_version = format_tool_version(result.ffprobe.version);
    if (result.ffmpeg.version != result.ffprobe.version) {
        result.warnings.push_back(std::format("FFmpeg '{}' ({}) and ffprobe '{}' ({}) report different versions.",
            path_to_text(result.ffmpeg.selected_path),
            ffmpeg_version,
            path_to_text(result.ffprobe.selected_path),
            ffprobe_version));
    }
    if (tool_root(result.ffmpeg.selected_path) != tool_root(result.ffprobe.selected_path)) {
        result.warnings.push_back(std::format("FFmpeg '{}' ({}) and ffprobe '{}' ({}) come from different roots.",
            path_to_text(result.ffmpeg.selected_path),
            ffmpeg_version,
            path_to_text(result.ffprobe.selected_path),
            ffprobe_version));
    }
    result.success = true;
    return result;
}

auto discover_tool(
    const ToolKind kind, const Path& configured_path, const ToolDiscoveryOptions& options) -> ToolResolution {
    return MediaToolResolver {options}.resolve(kind, configured_path);
}

auto discover_media_tools(const Path& configured_ffmpeg,
    const Path& configured_ffprobe,
    const ToolDiscoveryOptions& options) -> ToolDiscoveryResult {
    return MediaToolResolver {options}.resolve_pair(configured_ffmpeg, configured_ffprobe);
}

} // namespace vidchopper
