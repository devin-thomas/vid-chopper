#include "cli/batch_resolver.hpp"

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

namespace vidchopper {

namespace {

enum class PathKind : u8 {
    Invalid = 0,
    File = 1,
    Directory = 2,
};

using PathGroups = std::map<std::string, std::vector<Path>>;

constexpr auto source_extensions = std::to_array<std::string_view>({".mp4", ".mkv", ".mov"});
constexpr auto chapter_extensions = std::to_array<std::string_view>({".json", ".yaml", ".yml"});

[[nodiscard]] auto path_sort_key(const Path& path) -> std::string {
    return to_lower_copy(path.filename().string());
}

[[nodiscard]] auto path_less(const Path& left, const Path& right) -> bool {
    const std::string left_key = path_sort_key(left);
    const std::string right_key = path_sort_key(right);
    if (left_key != right_key) {
        return left_key < right_key;
    }
    return left.string() < right.string();
}

[[nodiscard]] auto has_extension(const Path& path, const std::span<const std::string_view> extensions) -> bool {
    const std::string extension = to_lower_copy(path.extension().string());
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
        errors.push_back(std::format("Could not inspect {} path {}: {}.", label, path.string(), error.message()));
        return PathKind::Invalid;
    }
    if (!exists) {
        errors.push_back(std::format("{} path does not exist: {}.", label, path.string()));
        return PathKind::Invalid;
    }

    const bool regular_file = std::filesystem::is_regular_file(path, error);
    if (error) {
        errors.push_back(std::format("Could not inspect {} path {}: {}.", label, path.string(), error.message()));
        return PathKind::Invalid;
    }
    if (regular_file) {
        return PathKind::File;
    }

    const bool directory = std::filesystem::is_directory(path, error);
    if (error) {
        errors.push_back(std::format("Could not inspect {} path {}: {}.", label, path.string(), error.message()));
        return PathKind::Invalid;
    }
    if (directory) {
        return PathKind::Directory;
    }

    errors.push_back(std::format("{} path is neither a regular file nor a directory: {}.", label, path.string()));
    return PathKind::Invalid;
}

template <typename Predicate>
[[nodiscard]] auto collect_directory_files(const Path& directory,
    const std::string_view label,
    Predicate supported,
    std::vector<std::string>& errors) -> std::vector<Path> {
    auto files = std::vector<Path> {};
    auto iterator_error = std::error_code {};
    auto iterator = std::filesystem::directory_iterator {directory, iterator_error};
    const auto end = std::filesystem::directory_iterator {};
    if (iterator_error) {
        errors.push_back(
            std::format("Could not read {} directory {}: {}.", label, directory.string(), iterator_error.message()));
        return files;
    }

    while (iterator != end) {
        const std::filesystem::directory_entry& entry = *iterator;
        auto entry_error = std::error_code {};
        if (entry.is_regular_file(entry_error)) {
            if (supported(entry.path())) {
                files.push_back(entry.path());
            }
        } else if (entry_error) {
            errors.push_back(std::format(
                "Could not inspect {} directory entry {}: {}.", label, entry.path().string(), entry_error.message()));
        }

        iterator.increment(iterator_error);
        if (iterator_error) {
            errors.push_back(std::format(
                "Could not read {} directory {}: {}.", label, directory.string(), iterator_error.message()));
            break;
        }
    }

    std::ranges::sort(files, path_less);
    if (files.empty()) {
        errors.push_back(std::format("No supported {} files found in directory: {}.", label, directory.string()));
    }
    return files;
}

[[nodiscard]] auto collect_sources(const Path& directory, std::vector<std::string>& errors) -> std::vector<Path> {
    return collect_directory_files(directory, "Source", is_source_file, errors);
}

[[nodiscard]] auto collect_chapter_files(const Path& directory, std::vector<std::string>& errors) -> std::vector<Path> {
    return collect_directory_files(directory, "ChapterFile", is_chapter_file, errors);
}

[[nodiscard]] auto group_by_stem(const std::vector<Path>& paths) -> PathGroups {
    auto groups = PathGroups {};
    for (const Path& path : paths) {
        groups[to_lower_copy(path.stem().string())].push_back(path);
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

        auto message = std::format("Duplicate {} stem \"{}\" found:", label, paths.front().stem().string());
        for (const Path& path : paths) {
            message += "\n- " + path.string();
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
                errors.push_back(std::format("Missing ChapterFile for Source: {}.", source_path.string()));
            }
        }
    }
    for (const auto& [stem, chapter_paths] : chapter_groups) {
        if (!source_groups.contains(stem)) {
            for (const Path& chapter_path : chapter_paths) {
                errors.push_back(std::format("Orphan ChapterFile: {}.", chapter_path.string()));
            }
        }
    }

    if (!errors.empty()) {
        return;
    }

    jobs.reserve(sources.size());
    for (const Path& source : sources) {
        const std::string stem = to_lower_copy(source.stem().string());
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
            "Unsupported Source extension: {}. Supported extensions: .mp4, .mkv, .mov.", request.source_path.string()));
    }
    if (!request.use_embedded_chapters && chapter_kind == PathKind::File
        && !is_chapter_file(*request.chapter_source_path)) {
        result.errors.push_back(
            std::format("Unsupported ChapterFile extension: {}. Supported extensions: .json, .yaml, .yml.",
                request.chapter_source_path->string()));
    }

    if (source_kind == PathKind::Invalid || (!request.use_embedded_chapters && chapter_kind == PathKind::Invalid)) {
        return result;
    }

    auto sources = std::vector<Path> {};
    if (source_kind == PathKind::File) {
        sources.push_back(request.source_path);
    } else {
        sources = collect_sources(request.source_path, result.errors);
        append_duplicate_errors(group_by_stem(sources), "Source", result.errors);
    }

    if (request.use_embedded_chapters) {
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
            request.source_path.string(),
            chapter_source.string()));
        return result;
    }

    const std::vector<Path> chapter_files = collect_chapter_files(chapter_source, result.errors);
    auto jobs = std::vector<BatchJob> {};
    resolve_directory_pairing(sources, chapter_files, jobs, result.errors);
    if (result.errors.empty()) {
        result.jobs = std::move(jobs);
    }
    return result;
}

} // namespace vidchopper
