#include "cli/batch_resolver.hpp"

#include "core/path_utils.hpp"
#include "core/string_utils.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <format>
#include <map>
#include <ranges>
#include <span>
#include <string_view>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace vidchopper {

namespace {

enum class PathKind : u8 {
    Invalid = 0,
    File = 1,
    Directory = 2,
};

using NativeString = Path::string_type;

[[nodiscard]] auto ordinal_case_insensitive_compare(const NativeString& left, const NativeString& right) -> int {
#ifdef _WIN32
    const int result = CompareStringOrdinal(
        left.data(), static_cast<int>(left.size()), right.data(), static_cast<int>(right.size()), TRUE);
    if (result == CSTR_LESS_THAN) {
        return -1;
    }
    if (result == CSTR_GREATER_THAN) {
        return 1;
    }
    if (result == 0) {
        if (left < right) {
            return -1;
        }
        if (right < left) {
            return 1;
        }
    }
    return 0;
#else
    const std::string left_key = to_lower_copy(left);
    const std::string right_key = to_lower_copy(right);
    if (left_key < right_key) {
        return -1;
    }
    if (right_key < left_key) {
        return 1;
    }
    return 0;
#endif
}

struct OrdinalCaseInsensitiveLess {
    [[nodiscard]] auto operator()(const NativeString& left, const NativeString& right) const -> bool {
        return ordinal_case_insensitive_compare(left, right) < 0;
    }
};

using PathGroups = std::map<NativeString, std::vector<Path>, OrdinalCaseInsensitiveLess>;

struct DirectoryInventory {
    std::vector<Path> files;
    std::vector<std::string> errors;
    bool complete {true};
};

constexpr auto source_extensions = std::to_array<std::string_view>({".mp4", ".mkv", ".mov"});
constexpr auto chapter_extensions = std::to_array<std::string_view>({".json", ".yaml", ".yml"});

[[nodiscard]] auto path_less(const Path& left, const Path& right) -> bool {
    const NativeString left_name = left.filename().native();
    const NativeString right_name = right.filename().native();
    const int insensitive_order = ordinal_case_insensitive_compare(left_name, right_name);
    if (insensitive_order != 0) {
        return insensitive_order < 0;
    }
    return left.native() < right.native();
}

[[nodiscard]] auto has_extension(const Path& path, const std::span<const std::string_view> extensions) -> bool {
    const std::string extension = to_lower_copy(path_to_utf8(path.extension()));
    return std::ranges::find(extensions, extension) != extensions.end();
}

[[nodiscard]] auto is_source_file(const Path& path) -> bool {
    return has_extension(path, source_extensions);
}

[[nodiscard]] auto is_chapter_file(const Path& path) -> bool {
    return has_extension(path, chapter_extensions);
}

[[nodiscard]] auto inspect_path(
    const Path& path, const std::string_view label, std::vector<std::string>& errors) -> PathKind {
    auto error = std::error_code {};
    const bool exists = std::filesystem::exists(path, error);
    if (error) {
        errors.push_back(std::format("Could not inspect {} path {}: {}.", label, path_to_utf8(path), error.message()));
        return PathKind::Invalid;
    }
    if (!exists) {
        errors.push_back(std::format("{} path does not exist: {}.", label, path_to_utf8(path)));
        return PathKind::Invalid;
    }

    const bool regular_file = std::filesystem::is_regular_file(path, error);
    if (error) {
        errors.push_back(std::format("Could not inspect {} path {}: {}.", label, path_to_utf8(path), error.message()));
        return PathKind::Invalid;
    }
    if (regular_file) {
        return PathKind::File;
    }

    const bool directory = std::filesystem::is_directory(path, error);
    if (error) {
        errors.push_back(std::format("Could not inspect {} path {}: {}.", label, path_to_utf8(path), error.message()));
        return PathKind::Invalid;
    }
    if (directory) {
        return PathKind::Directory;
    }

    errors.push_back(std::format("{} path is neither a regular file nor a directory: {}.", label, path_to_utf8(path)));
    return PathKind::Invalid;
}

[[nodiscard]] auto scan_directory(const Path& directory) -> DirectoryScanResult {
    auto result = DirectoryScanResult {};
    auto iterator_error = std::error_code {};
    auto iterator = std::filesystem::directory_iterator {directory, iterator_error};
    const auto end = std::filesystem::directory_iterator {};
    if (iterator_error) {
        result.complete = false;
        result.failures.push_back(DirectoryScanFailure {.message = iterator_error.message()});
        return result;
    }

    while (iterator != end) {
        const std::filesystem::directory_entry& entry = *iterator;
        auto entry_error = std::error_code {};
        if (entry.is_regular_file(entry_error)) {
            result.regular_files.push_back(entry.path());
        } else if (entry_error) {
            result.complete = false;
            result.failures.push_back(
                DirectoryScanFailure {.entry_path = entry.path(), .message = entry_error.message()});
        }

        iterator.increment(iterator_error);
        if (iterator_error) {
            result.complete = false;
            result.failures.push_back(DirectoryScanFailure {.message = iterator_error.message()});
            break;
        }
    }

    return result;
}

template <typename Predicate>
[[nodiscard]] auto collect_directory_files(const Path& directory,
    const std::string_view label,
    Predicate supported,
    const DirectoryScanner& scanner) -> DirectoryInventory {
    const DirectoryScanResult scan = scanner ? scanner(directory) : scan_directory(directory);
    auto inventory = DirectoryInventory {.complete = scan.complete};
    for (const DirectoryScanFailure& failure : scan.failures) {
        if (failure.entry_path.has_value()) {
            inventory.errors.push_back(std::format(
                "Could not inspect {} directory entry {}: {}.", label, path_to_utf8(*failure.entry_path), failure.message));
        } else {
            inventory.errors.push_back(
                std::format("Could not read {} directory {}: {}.", label, path_to_utf8(directory), failure.message));
        }
    }
    if (!inventory.complete && inventory.errors.empty()) {
        inventory.errors.push_back(
            std::format("Could not completely read {} directory {}.", label, path_to_utf8(directory)));
    }
    std::ranges::sort(inventory.errors);

    for (const Path& path : scan.regular_files) {
        if (supported(path)) {
            inventory.files.push_back(path);
        }
    }

    std::ranges::sort(inventory.files, path_less);
    if (inventory.complete && inventory.files.empty()) {
        inventory.errors.push_back(
            std::format("No supported {} files found in directory: {}.", label, path_to_utf8(directory)));
    }
    return inventory;
}

[[nodiscard]] auto collect_sources(const Path& directory, const DirectoryScanner& scanner) -> DirectoryInventory {
    return collect_directory_files(directory, "Source", is_source_file, scanner);
}

[[nodiscard]] auto collect_chapter_files(const Path& directory, const DirectoryScanner& scanner) -> DirectoryInventory {
    return collect_directory_files(directory, "ChapterFile", is_chapter_file, scanner);
}

[[nodiscard]] auto group_by_stem(const std::vector<Path>& paths) -> PathGroups {
    auto groups = PathGroups {};
    for (const Path& path : paths) {
        groups[path.stem().native()].push_back(path);
    }
    return groups;
}

auto append_duplicate_errors(
    const PathGroups& groups, const std::string_view label, std::vector<std::string>& errors) -> void {
    for (const auto& group : groups) {
        const std::vector<Path>& paths = group.second;
        if (paths.size() < 2) {
            continue;
        }

        auto message = std::format("Duplicate {} stem \"{}\" found:", label, path_to_utf8(paths.front().stem()));
        for (const Path& path : paths) {
            message += "\n- " + path_to_utf8(path);
        }
        errors.push_back(std::move(message));
    }
}

auto resolve_directory_pairing(const std::vector<Path>& sources,
    const std::vector<Path>& chapter_files,
    std::vector<BatchJob>& jobs,
    std::vector<std::string>& errors) -> void {
    const PathGroups source_groups = group_by_stem(sources);
    const PathGroups chapter_groups = group_by_stem(chapter_files);
    append_duplicate_errors(chapter_groups, "ChapterFile", errors);

    if (sources.size() != chapter_files.size()) {
        errors.push_back(
            std::format("N:N count mismatch: {} Sources and {} ChapterFiles.", sources.size(), chapter_files.size()));
    }

    for (const auto& [stem, source_paths] : source_groups) {
        if (!chapter_groups.contains(stem)) {
            for (const Path& source_path : source_paths) {
                errors.push_back(std::format("Missing ChapterFile for Source: {}.", path_to_utf8(source_path)));
            }
        }
    }
    for (const auto& [stem, chapter_paths] : chapter_groups) {
        if (!source_groups.contains(stem)) {
            for (const Path& chapter_path : chapter_paths) {
                errors.push_back(std::format("Orphan ChapterFile: {}.", path_to_utf8(chapter_path)));
            }
        }
    }

    if (!errors.empty()) {
        return;
    }

    jobs.reserve(sources.size());
    for (const Path& source : sources) {
        const NativeString stem = source.stem().native();
        jobs.push_back(BatchJob {
            .source_path = source,
            .chapter_config_path = chapter_groups.at(stem).front(),
        });
    }
}

} // namespace

auto BatchResolution::ok() const noexcept -> bool {
    return errors.empty();
}

auto resolve_batch(const BatchResolveRequest& request) -> BatchResolution {
    auto result = BatchResolution {};
    const PathKind source_kind = inspect_path(request.source_path, "Source", result.errors);

    auto chapter_kind = PathKind::Invalid;
    if (request.use_embedded_chapters) {
        if (request.chapter_source_path.has_value()) {
            result.errors.push_back("Choose exactly one chapter source: a ChapterFile path or embedded chapters.");
        }
    } else if (!request.chapter_source_path.has_value()) {
        result.errors.push_back("A ChapterFile path or explicit embedded mode is required.");
    } else {
        chapter_kind = inspect_path(*request.chapter_source_path, "ChapterFile", result.errors);
    }

    if (source_kind == PathKind::File && !is_source_file(request.source_path)) {
        result.errors.push_back(std::format(
            "Unsupported Source extension: {}. Supported extensions: .mp4, .mkv, .mov.", path_to_utf8(request.source_path)));
    }
    if (!request.use_embedded_chapters && chapter_kind == PathKind::File
        && !is_chapter_file(*request.chapter_source_path)) {
        result.errors.push_back(
            std::format("Unsupported ChapterFile extension: {}. Supported extensions: .json, .yaml, .yml.",
                path_to_utf8(*request.chapter_source_path)));
    }

    if (source_kind == PathKind::Invalid || (!request.use_embedded_chapters && chapter_kind == PathKind::Invalid)) {
        return result;
    }

    auto source_inventory = DirectoryInventory {};
    if (source_kind == PathKind::File) {
        source_inventory.files.push_back(request.source_path);
    } else {
        source_inventory = collect_sources(request.source_path, request.directory_scanner);
        result.errors.insert(result.errors.end(), source_inventory.errors.begin(), source_inventory.errors.end());
    }
    const std::vector<Path>& sources = source_inventory.files;

    if (request.use_embedded_chapters) {
        if (!source_inventory.complete) {
            return result;
        }
        append_duplicate_errors(group_by_stem(sources), "Source", result.errors);
        if (!result.errors.empty()) {
            return result;
        }
        result.jobs.reserve(sources.size());
        for (const Path& source : sources) {
            result.jobs.push_back(BatchJob {.source_path = source});
        }
        return result;
    }

    const Path& chapter_source = *request.chapter_source_path;
    if (chapter_kind == PathKind::File) {
        if (!source_inventory.complete) {
            return result;
        }
        append_duplicate_errors(group_by_stem(sources), "Source", result.errors);
        if (!result.errors.empty()) {
            return result;
        }

        result.jobs.reserve(sources.size());
        for (const Path& source : sources) {
            result.jobs.push_back(BatchJob {
                .source_path = source,
                .chapter_config_path = chapter_source,
            });
        }
        return result;
    }

    if (source_kind == PathKind::File) {
        result.errors.push_back(std::format("1:N is not supported: Source {} cannot use ChapterFile directory {}.",
            path_to_utf8(request.source_path),
            path_to_utf8(chapter_source)));
        return result;
    }

    const DirectoryInventory chapter_inventory = collect_chapter_files(chapter_source, request.directory_scanner);
    result.errors.insert(result.errors.end(), chapter_inventory.errors.begin(), chapter_inventory.errors.end());
    if (!source_inventory.complete || !chapter_inventory.complete) {
        return result;
    }

    append_duplicate_errors(group_by_stem(sources), "Source", result.errors);
    auto jobs = std::vector<BatchJob> {};
    resolve_directory_pairing(sources, chapter_inventory.files, jobs, result.errors);
    if (result.errors.empty()) {
        result.jobs = std::move(jobs);
    }
    return result;
}

} // namespace vidchopper
