#include "services/manifest_writer.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <system_error>

namespace vidchopper {

namespace {

using Json = nlohmann::json;

[[nodiscard]] auto csv_escape(const std::string& value) -> std::string {
    auto escaped = std::string {"\""};
    for (const char character : value) {
        if (character == '\"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(character);
        }
    }
    escaped.push_back('\"');
    return escaped;
}

[[nodiscard]] auto segment_json(const PlannedExportSegment& planned, const RenderedSegment* rendered) -> Json {
    auto value = Json {
        {"index", planned.chapter_index + 1},
        {"name", planned.chapter.name},
        {"startMs", planned.chapter.start_ms},
        {"endMs", planned.chapter.end_ms},
        {"durationMs", planned.chapter.end_ms - planned.chapter.start_ms},
        {"outputPath", planned.output_path.string()},
        {"plannedCommand", planned.command},
    };
    if (rendered != nullptr) {
        value["processState"] = process_exit_state_name(rendered->process.state);
        value["processExitCode"] = rendered->process.exit_code;
        value["durationVerified"] = rendered->duration_verified;
        value["actualDurationMs"] = rendered->actual_duration_ms;
        value["skipped"] = rendered->skipped;
        value["overwroteExisting"] = rendered->overwrote_existing;
        value["error"] = !rendered->verification_error.empty()
            ? rendered->verification_error
            : (rendered->process.error_message.empty() ? rendered->process.standard_error
                                                       : rendered->process.error_message);
    } else {
        value["processState"] = "planned";
        value["skipped"] = false;
        value["overwroteExisting"] = false;
    }
    return value;
}

[[nodiscard]] auto job_json(const ResolvedExportJob& job, const ExportJobResult* result) -> Json {
    auto value = Json {
        {"schemaVersion", 1},
        {"timestampEpochMs",
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count()},
        {"source", job.metadata.source_path.string()},
        {"chapterSource",
            job.uses_embedded_chapters
                ? Json {"embedded"}
                : Json {job.chapter_source_path.has_value() ? job.chapter_source_path->string() : "unspecified"}},
        {"outputDirectory", job.output_directory.string()},
        {"settings",
            {{"x264Crf", job.settings.x264_crf},
                {"nvencCq", job.settings.nvenc_cq},
                {"x264Preset", job.settings.x264_preset},
                {"nvencPreset", job.settings.nvenc_preset},
                {"threads", job.settings.ffmpeg_threads},
                {"overwriteMode", static_cast<u8>(job.settings.overwrite_mode)}}},
    };
    auto segments = Json::array();
    for (const PlannedExportSegment& planned : job.segments) {
        const RenderedSegment* rendered = nullptr;
        if (result != nullptr) {
            for (const RenderedSegment& candidate : result->segments) {
                if (candidate.chapter_index == planned.chapter_index) {
                    rendered = &candidate;
                    break;
                }
            }
        }
        segments.push_back(segment_json(planned, rendered));
    }
    value["segments"] = std::move(segments);
    value["jobStatus"] = result == nullptr ? "planned" : (result->ok() ? "success" : "failed");
    if (result != nullptr && !result->error_message.empty()) {
        value["error"] = result->error_message;
    }
    return value;
}

auto atomic_write(const Path& target, const std::string& text, std::vector<std::string>& errors) -> bool {
    auto error = std::error_code {};
    const Path parent = target.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            errors.push_back("Could not create manifest directory '" + parent.string() + "': " + error.message());
            return false;
        }
    }

    const Path temporary = target.string() + ".tmp";
    auto stream = std::ofstream {temporary, std::ios::binary | std::ios::trunc};
    if (!stream.is_open()) {
        errors.push_back("Could not open manifest temporary file '" + temporary.string() + "'.");
        return false;
    }
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.flush();
    if (!stream.good()) {
        stream.close();
        std::filesystem::remove(temporary, error);
        errors.push_back("Could not completely write manifest temporary file '" + temporary.string() + "'.");
        return false;
    }
    stream.close();
    if (stream.fail()) {
        std::filesystem::remove(temporary, error);
        errors.push_back("Could not close manifest temporary file '" + temporary.string() + "'.");
        return false;
    }

    error.clear();
    const auto written_size = std::filesystem::file_size(temporary, error);
    if (error || written_size != text.size()) {
        const std::string detail = error ? error.message() : "written size did not match the requested content";
        std::filesystem::remove(temporary, error);
        errors.push_back("Could not verify complete manifest temporary file '" + temporary.string() + "': " + detail);
        return false;
    }

    error.clear();
    std::filesystem::remove(target, error);
    error.clear();
    std::filesystem::rename(temporary, target, error);
    if (error) {
        auto cleanup_error = std::error_code {};
        std::filesystem::remove(temporary, cleanup_error);
        errors.push_back("Could not replace manifest '" + target.string() + "': " + error.message());
        return false;
    }
    return true;
}

auto write_csv(const Path& target,
    const ResolvedExportJob& job,
    const ExportJobResult* result,
    std::vector<std::string>& errors) -> bool {
    auto text = std::string {"index,name,start_ms,end_ms,output_path,process_state,skipped,error\n"};
    for (const PlannedExportSegment& planned : job.segments) {
        const RenderedSegment* rendered = nullptr;
        if (result != nullptr) {
            for (const RenderedSegment& candidate : result->segments) {
                if (candidate.chapter_index == planned.chapter_index) {
                    rendered = &candidate;
                    break;
                }
            }
        }
        const std::string state = rendered == nullptr ? "planned" : process_exit_state_name(rendered->process.state);
        const std::string error = rendered == nullptr
            ? ""
            : (!rendered->verification_error.empty() ? rendered->verification_error : rendered->process.error_message);
        text += std::to_string(planned.chapter_index + 1) + "," + csv_escape(planned.chapter.name) + ","
            + std::to_string(planned.chapter.start_ms) + "," + std::to_string(planned.chapter.end_ms) + ","
            + csv_escape(planned.output_path.string()) + "," + csv_escape(state) + ","
            + (rendered != nullptr && rendered->skipped ? "true" : "false") + "," + csv_escape(error) + "\n";
    }
    return atomic_write(target, text, errors);
}

auto write_aggregate_csv(const Path& target,
    const std::vector<ResolvedExportJob>& jobs,
    const ExportRunResult& run_result,
    std::vector<std::string>& errors) -> bool {
    auto text = std::string {"source,index,name,start_ms,end_ms,output_path,process_state,skipped,error\n"};
    for (size_t job_index = 0; job_index < jobs.size(); ++job_index) {
        const ResolvedExportJob& job = jobs[job_index];
        const ExportJobResult* result = job_index < run_result.jobs.size() ? &run_result.jobs[job_index] : nullptr;
        for (const PlannedExportSegment& planned : job.segments) {
            const RenderedSegment* rendered = nullptr;
            if (result != nullptr) {
                for (const RenderedSegment& candidate : result->segments) {
                    if (candidate.chapter_index == planned.chapter_index) {
                        rendered = &candidate;
                        break;
                    }
                }
            }
            const std::string state =
                rendered == nullptr ? "planned" : process_exit_state_name(rendered->process.state);
            const std::string error = rendered == nullptr
                ? ""
                : (!rendered->verification_error.empty() ? rendered->verification_error
                                                         : rendered->process.error_message);
            text += csv_escape(job.metadata.source_path.string()) + "," + std::to_string(planned.chapter_index + 1)
                + "," + csv_escape(planned.chapter.name) + "," + std::to_string(planned.chapter.start_ms) + ","
                + std::to_string(planned.chapter.end_ms) + "," + csv_escape(planned.output_path.string()) + ","
                + csv_escape(state) + "," + (rendered != nullptr && rendered->skipped ? "true" : "false") + ","
                + csv_escape(error) + "\n";
        }
    }
    return atomic_write(target, text, errors);
}

} // namespace

auto ManifestJobWriteResult::ok() const noexcept -> bool {
    return success && errors.empty();
}

auto ManifestWriteResult::ok() const noexcept -> bool {
    return success && errors.empty();
}

auto write_manifests(const std::vector<ResolvedExportJob>& planned_jobs,
    const ExportRunResult& run_result,
    const std::optional<Path>& aggregate_json_path,
    const std::optional<Path>& aggregate_csv_path) -> ManifestWriteResult {
    auto result = ManifestWriteResult {.success = true};
    result.jobs.reserve(planned_jobs.size());
    for (size_t index = 0; index < planned_jobs.size(); ++index) {
        const ResolvedExportJob& job = planned_jobs[index];
        const ExportJobResult* export_job = index < run_result.jobs.size() ? &run_result.jobs[index] : nullptr;
        auto manifest_job = ManifestJobWriteResult {
            .job_index = index,
            .source_path = job.metadata.source_path,
            .success = true,
        };
        if (export_job != nullptr) {
            for (const RenderedSegment& segment : export_job->segments) {
                if (segment.ok() && !segment.skipped) {
                    manifest_job.preserved_media_paths.push_back(segment.output_path);
                    result.preserved_media_paths.push_back(segment.output_path);
                }
            }
        }
        if (job.settings.write_json_manifest) {
            const Path target = job.output_directory / "vidchopper-manifest.json";
            if (atomic_write(target, job_json(job, export_job).dump(2), manifest_job.errors)) {
                manifest_job.written_paths.push_back(target);
                result.written_paths.push_back(target);
            }
        }
        if (job.settings.write_csv_manifest) {
            const Path target = job.output_directory / "vidchopper-manifest.csv";
            if (write_csv(target, job, export_job, manifest_job.errors)) {
                manifest_job.written_paths.push_back(target);
                result.written_paths.push_back(target);
            }
        }
        manifest_job.success = manifest_job.errors.empty();
        for (const std::string& error : manifest_job.errors) {
            result.errors.push_back("Manifest failure for job " + std::to_string(index + 1) + " ('"
                + job.metadata.source_path.string() + "'): " + error);
        }
        result.jobs.push_back(std::move(manifest_job));
    }
    if (aggregate_json_path.has_value()) {
        auto aggregate = Json {
            {"schemaVersion", 1},
            {"jobCount", planned_jobs.size()},
            {"successful", run_result.ok()},
            {"jobs", Json::array()},
        };
        for (size_t index = 0; index < planned_jobs.size(); ++index) {
            const ExportJobResult* job_result = index < run_result.jobs.size() ? &run_result.jobs[index] : nullptr;
            aggregate["jobs"].push_back(job_json(planned_jobs[index], job_result));
        }
        if (atomic_write(*aggregate_json_path, aggregate.dump(2), result.errors)) {
            result.written_paths.push_back(*aggregate_json_path);
        }
    }
    if (aggregate_csv_path.has_value()
        && write_aggregate_csv(*aggregate_csv_path, planned_jobs, run_result, result.errors)) {
        result.written_paths.push_back(*aggregate_csv_path);
    }
    result.success = result.errors.empty();
    return result;
}

} // namespace vidchopper
