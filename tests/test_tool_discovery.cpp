#include "services/tool_discovery.hpp"
#include "core/path_utils.hpp"
#include "test_support.hpp"

#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <utility>

using namespace vidchopper;

namespace {

[[nodiscard]] auto contains(const std::string_view text, const std::string_view needle) -> bool {
    return text.find(needle) != std::string_view::npos;
}

[[nodiscard]] auto fixture_name(const std::string_view stem) -> std::string {
#ifdef _WIN32
    return std::string {stem} + ".exe";
#else
    return std::string {stem};
#endif
}

[[nodiscard]] auto path_separator() noexcept -> std::string_view {
#ifdef _WIN32
    return ";";
#else
    return ":";
#endif
}

[[nodiscard]] auto copy_fixture(const Path& source, const Path& directory, const std::string_view stem) -> Path {
    const Path destination = directory / fixture_name(stem);
    const bool copied =
        std::filesystem::copy_file(source, destination, std::filesystem::copy_options::overwrite_existing);
    test_support::expect_true(copied, "tool fixture should be copied");
#ifndef _WIN32
    static_cast<void>(std::filesystem::permissions(
        destination, std::filesystem::perms::owner_exec, std::filesystem::perm_options::add));
#endif
    return destination;
}

[[nodiscard]] auto version_output(const Path& executable) -> std::string {
    const std::string name = path_to_utf8(executable.filename());
    const std::string tool = contains(name, "ffprobe") ? "ffprobe" : "ffmpeg";
    if (contains(name, "unsupported-old")) {
        return std::format("{} version 6.0.0", tool);
    }
    if (contains(name, "supported-old")) {
        return std::format("{} version 6.1.0", tool);
    }
    if (contains(name, "unsupported-new")) {
        return std::format("{} version 9.0.0", tool);
    }
    if (contains(name, "supported-new")) {
        return std::format("{} version 8.9.0", tool);
    }
    if (contains(name, "unparseable")) {
        return std::format("{} version development-build", tool);
    }
    if (contains(name, "version-eight")) {
        return std::format("{} version 8.0.1", tool);
    }
    return std::format("{} version 7.1.2", tool);
}

[[nodiscard]] auto fake_executor() -> ProcessExecutor {
    return [](const ProcessRequest& request) -> ProcessResult {
        test_support::expect_eq(
            request.arguments, std::vector<std::string> {"-version"}, "resolver should use -version");
        return ProcessResult {
            .state = ProcessExitState::Success,
            .standard_error = version_output(request.executable),
        };
    };
}

} // namespace

auto main(const int argument_count, char** arguments) -> int {
    test_support::expect_true(argument_count > 0, "test should receive its executable path");
    auto path_error = std::error_code {};
    const Path self = std::filesystem::absolute(Path {arguments[0]}, path_error);
    test_support::expect_true(!path_error, "test executable path should be resolvable");

    test_support::expect_eq(parse_tool_version("ffmpeg version 6.1.0 Copyright", ToolKind::Ffmpeg),
        std::optional<ToolVersion> {ToolVersion {.major = 6, .minor = 1, .patch = 0}},
        "6.1 should parse");
    test_support::expect_eq(parse_tool_version("ffprobe version 7.0.2", ToolKind::Ffprobe),
        std::optional<ToolVersion> {ToolVersion {.major = 7, .minor = 0, .patch = 2}},
        "7.x should parse");
    test_support::expect_eq(parse_tool_version("ffmpeg version 8.2", ToolKind::Ffmpeg),
        std::optional<ToolVersion> {ToolVersion {.major = 8, .minor = 2, .patch = 0}},
        "8.x should parse");
    test_support::expect_true(!parse_tool_version("not an ffmpeg version", ToolKind::Ffmpeg).has_value(),
        "unparseable output should be rejected");
    test_support::expect_true(
        !is_supported_tool_version(ToolVersion {.major = 6, .minor = 0}), "6.0 should be outside the supported range");
    test_support::expect_true(
        is_supported_tool_version(ToolVersion {.major = 6, .minor = 1}), "6.1 should be supported");
    test_support::expect_true(
        is_supported_tool_version(ToolVersion {.major = 8, .minor = 9}), "8.x should be supported");
    test_support::expect_true(
        !is_supported_tool_version(ToolVersion {.major = 9, .minor = 0}), "9.x should be outside the supported range");

    const Path root = std::filesystem::temp_directory_path() / "vidchopper-tool-discovery-test";
    static_cast<void>(std::filesystem::remove_all(root));
    static_cast<void>(std::filesystem::create_directories(root));
    const Path path_directory = root / "path";
    const Path homebrew_directory = root / "homebrew";
    const Path standard_directory = root / "standard";
    static_cast<void>(std::filesystem::create_directories(path_directory));
    static_cast<void>(std::filesystem::create_directories(homebrew_directory));
    static_cast<void>(std::filesystem::create_directories(standard_directory));

    const Path configured_ffmpeg = copy_fixture(self, root, "configured-ffmpeg");
    const Path configured_ffprobe = copy_fixture(self, root, "configured-ffprobe");
    const Path path_ffmpeg = copy_fixture(self, path_directory, "ffmpeg");
    const Path path_ffprobe = copy_fixture(self, path_directory, "ffprobe");
    const Path homebrew_ffmpeg = copy_fixture(self, homebrew_directory, "ffmpeg");
    const Path standard_ffmpeg = copy_fixture(self, standard_directory, "ffmpeg");
    (void)path_ffprobe;
    (void)homebrew_ffmpeg;
    (void)standard_ffmpeg;

    const ToolDiscoveryOptions deterministic_options {
        .executor = fake_executor(),
        .path_environment = path_to_utf8(path_directory) + std::string {path_separator()}
            + path_to_utf8(path_directory),
        .additional_homebrew_paths = {homebrew_directory},
        .additional_standard_paths = {standard_directory},
        .use_platform_defaults = false,
    };
    const ToolResolution configured = discover_tool(ToolKind::Ffmpeg, configured_ffmpeg, deterministic_options);
    test_support::expect_true(configured.ok(), "a configured executable should resolve");
    test_support::expect_true(
        std::filesystem::equivalent(configured.selected_path, configured_ffmpeg), "configured executable should win");
    test_support::expect_eq(
        configured.source, ToolDiscoverySource::ConfiguredPath, "configured executable should report its source");

    const ToolResolution from_path = discover_tool(ToolKind::Ffmpeg, {}, deterministic_options);
    test_support::expect_true(from_path.ok(), "PATH executable should resolve");
    test_support::expect_true(std::filesystem::equivalent(from_path.selected_path, path_ffmpeg),
        "PATH should precede Homebrew and standard paths");
    test_support::expect_eq(from_path.source, ToolDiscoverySource::Path, "PATH source should be reported");
    test_support::expect_eq(from_path.diagnostics.size(), size_t {1}, "duplicate PATH entries should be deduplicated");

    const ToolDiscoveryOptions homebrew_options {
        .executor = fake_executor(),
        .path_environment = std::string {},
        .additional_homebrew_paths = {homebrew_directory},
        .use_platform_defaults = false,
    };
    const ToolResolution from_homebrew = discover_tool(ToolKind::Ffmpeg, {}, homebrew_options);
    test_support::expect_true(from_homebrew.ok(), "Homebrew executable should resolve");
    test_support::expect_true(
        std::filesystem::equivalent(from_homebrew.selected_path, homebrew_ffmpeg), "Homebrew path should be searched");
    test_support::expect_eq(from_homebrew.source, ToolDiscoverySource::Homebrew, "Homebrew source should be reported");

    const ToolDiscoveryOptions standard_options {
        .executor = fake_executor(),
        .path_environment = std::string {},
        .additional_standard_paths = {standard_directory},
        .use_platform_defaults = false,
    };
    const ToolResolution from_standard = discover_tool(ToolKind::Ffmpeg, {}, standard_options);
    test_support::expect_true(from_standard.ok(), "standard Unix executable should resolve");
    test_support::expect_true(
        std::filesystem::equivalent(from_standard.selected_path, standard_ffmpeg), "standard path should be searched");
    test_support::expect_eq(
        from_standard.source, ToolDiscoverySource::StandardLocation, "standard source should be reported");

#ifdef _WIN32
    const Path unicode_directory = root / path_from_utf8("tool paths/工具 🎬");
    static_cast<void>(std::filesystem::create_directories(unicode_directory));
    const Path unicode_ffmpeg = copy_fixture(self, unicode_directory, "ffmpeg");
    const ToolDiscoveryOptions unicode_options {
        .executor = fake_executor(),
        .path_environment = path_to_utf8(unicode_directory),
        .use_platform_defaults = false,
    };
    const ToolResolution from_unicode_path = discover_tool(ToolKind::Ffmpeg, {}, unicode_options);
    test_support::expect_true(from_unicode_path.ok(), "UTF-8 Windows PATH entries should resolve");
    test_support::expect_true(std::filesystem::equivalent(from_unicode_path.selected_path, unicode_ffmpeg),
        "UTF-8 Windows PATH entries should preserve the native path");
#endif

    const Path unsupported_old = copy_fixture(self, root, "unsupported-old-ffmpeg");
    const ToolResolution old_result = discover_tool(ToolKind::Ffmpeg, unsupported_old, deterministic_options);
    test_support::expect_true(!old_result.ok(), "6.0 should be blocked");
    test_support::expect_true(contains(old_result.failure_reason, path_to_utf8(unsupported_old)),
        "unsupported diagnostics should identify the exact path");
    test_support::expect_true(contains(old_result.failure_reason, "6.1 through 8.x"),
        "unsupported diagnostics should state the supported range");

    const Path supported_old = copy_fixture(self, root, "supported-old-ffmpeg");
    const ToolResolution oldest_supported = discover_tool(ToolKind::Ffmpeg, supported_old, deterministic_options);
    test_support::expect_true(oldest_supported.ok(), "6.1 should remain an accepted fixture version");
    test_support::expect_eq(oldest_supported.version,
        ToolVersion {.major = 6, .minor = 1, .patch = 0},
        "the oldest supported fixture version should be retained");

    const Path unsupported_new = copy_fixture(self, root, "unsupported-new-ffmpeg");
    const ToolResolution new_result = discover_tool(ToolKind::Ffmpeg, unsupported_new, deterministic_options);
    test_support::expect_true(!new_result.ok(), "9.x should be blocked");

    const Path supported_new = copy_fixture(self, root, "supported-new-ffprobe");
    const ToolResolution newest_supported = discover_tool(ToolKind::Ffprobe, supported_new, deterministic_options);
    test_support::expect_true(newest_supported.ok(), "8.x should remain an accepted fixture version");
    test_support::expect_eq(newest_supported.version,
        ToolVersion {.major = 8, .minor = 9, .patch = 0},
        "the newest supported fixture version should be retained");

    const Path unparseable = copy_fixture(self, root, "unparseable-ffmpeg");
    const ToolResolution unparseable_result = discover_tool(ToolKind::Ffmpeg, unparseable, deterministic_options);
    test_support::expect_true(!unparseable_result.ok(), "unparseable versions should be blocked");

#ifndef _WIN32
    const Path non_executable = copy_fixture(self, root, "non-executable-ffmpeg");
    static_cast<void>(std::filesystem::permissions(
        non_executable, std::filesystem::perms::owner_exec, std::filesystem::perm_options::remove));
    const ToolResolution non_executable_result = discover_tool(ToolKind::Ffmpeg, non_executable, deterministic_options);
    test_support::expect_true(!non_executable_result.ok(), "non-executable tools should be blocked");
#endif

    const Path mismatched_ffmpeg = copy_fixture(self, root, "mismatch-ffmpeg");
    const Path mismatched_ffprobe = copy_fixture(self, root, "version-eight-ffprobe");
    const ToolDiscoveryResult pair = discover_media_tools(mismatched_ffmpeg, mismatched_ffprobe, deterministic_options);
    test_support::expect_true(pair.ok(), "two supported tools should resolve as a pair");
    test_support::expect_true(!pair.warnings.empty(), "supported version mismatches should warn");
    test_support::expect_true(contains(pair.warnings.front(), "7.1.2"), "mismatch warning should name ffmpeg version");
    test_support::expect_true(contains(pair.warnings.front(), "8.0.1"), "mismatch warning should name ffprobe version");

    static_cast<void>(std::filesystem::remove_all(root));
    return 0;
}
