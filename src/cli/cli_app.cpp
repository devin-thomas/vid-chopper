#include "cli/cli_app.hpp"

#include "cli/batch_resolver.hpp"
#include "cli/chapter_config.hpp"
#include "cli/chapter_source_policy.hpp"
#include "cli/cli_arguments.hpp"
#include "cli/cli_settings.hpp"
#include "cli/dry_run_renderer.hpp"
#include "cli/manifest_writer.hpp"
#include "cli/output_planner.hpp"

#include <filesystem>
#include <ostream>
#include <optional>
#include <utility>

namespace vidchopper {

namespace {

[[nodiscard]] auto probe_failure_exit_code() -> CliExitCode {
    return CliExitCode::ToolingError;
}

[[nodiscard]] auto export_exit_code(const ExportRunResult& result) -> CliExitCode {
    return result.exit_code == ExportExitCode::ToolingError ? CliExitCode::ToolingError : CliExitCode::ExportFailure;
}

} // namespace

auto run_cli(const CliRunRequest& request) -> CliExitCode {
    const CliParseResult parsed = parse_cli_arguments(request.arguments);
    if (!parsed.ok()) {
        request.error_output << parsed.error_message << "\n\n" << cli_usage();
        return CliExitCode::Error;
    }

    const CliArguments& cli_arguments = parsed.arguments;
    if (cli_arguments.command == CliCommand::Help) {
        request.output << cli_usage();
        return CliExitCode::Success;
    }

    const CliSettingsPaths settings_paths =
        resolve_cli_settings_paths(request.executable_path, cli_arguments.use_gui_config);
    if (!cli_arguments.dry_run && !ensure_cli_settings_file(settings_paths.cli_settings_path)) {
        request.error_output << "Could not create or open CLI settings file: ";
        request.error_output << settings_paths.cli_settings_path.string() << "\n";
        return CliExitCode::Error;
    }

    const CliResolvedSettings loaded_settings = load_cli_settings(settings_paths);
    const ExportSettings effective_settings = apply_cli_flag_overrides(loaded_settings.export_settings, cli_arguments);

    if (cli_arguments.input_paths.empty()) {
        request.error_output << "Expected an input video and one explicit chapter source.\n\n" << cli_usage();
        return CliExitCode::Error;
    }

    if (cli_arguments.config_paths.empty() && !cli_arguments.use_embedded_chapters) {
        const Path& source_path = cli_arguments.input_paths.front();
        auto path_error = std::error_code {};
        if (std::filesystem::is_regular_file(source_path, path_error) && !path_error) {
            const FfprobeResult probe =
                FfprobeClient {request.process_executor}.probe(effective_settings.ffprobe_path, source_path);
            if (!probe.ok()) {
                request.error_output << probe.error_message << "\n";
                return probe_failure_exit_code();
            }
            request.error_output << chapter_source_guidance(probe.metadata) << "\n";
            return CliExitCode::Error;
        }

        request.error_output << "Expected an input video and one explicit chapter source.\n\n" << cli_usage();
        return CliExitCode::Error;
    }

    const std::optional<Path> chapter_source_path =
        cli_arguments.config_paths.empty() ? std::nullopt : std::optional<Path> {cli_arguments.config_paths.front()};
    const BatchResolution batch = resolve_batch(BatchResolveRequest {
        .source_path = cli_arguments.input_paths.front(),
        .chapter_source_path = chapter_source_path,
        .use_embedded_chapters = cli_arguments.use_embedded_chapters,
        .directory_scanner = request.directory_scanner,
    });
    if (!batch.ok()) {
        for (const std::string& error : batch.errors) {
            request.error_output << error << "\n";
        }
        return CliExitCode::Error;
    }

    auto plan_inputs = std::vector<OutputPlanInput> {};
    plan_inputs.reserve(batch.jobs.size());
    if (cli_arguments.use_embedded_chapters) {
        for (const BatchJob& job : batch.jobs) {
            const FfprobeResult probe =
                FfprobeClient {request.process_executor}.probe(effective_settings.ffprobe_path, job.source_path);
            if (!probe.ok()) {
                request.error_output << probe.error_message << "\n";
                return probe_failure_exit_code();
            }
            if (probe.metadata.embedded_chapters.empty()) {
                if (batch.jobs.size() == 1) {
                    request.error_output << chapter_source_guidance(probe.metadata) << "\n";
                    return CliExitCode::Error;
                }
                request.output << "Skipping source without embedded chapters: " << job.source_path.string() << "\n";
                continue;
            }
            plan_inputs.push_back(OutputPlanInput {
                .metadata = probe.metadata,
                .uses_embedded_chapters = true,
                .chapters = probe.metadata.embedded_chapters,
                .settings = effective_settings,
            });
        }

        if (plan_inputs.empty()) {
            request.error_output << "No sources with embedded chapters were found.\n";
            return CliExitCode::Error;
        }
        request.output << "Chapter source: embedded chapters (explicit).\n";
    } else {
        for (const BatchJob& job : batch.jobs) {
            const FfprobeResult probe =
                FfprobeClient {request.process_executor}.probe(effective_settings.ffprobe_path, job.source_path);
            if (!probe.ok()) {
                request.error_output << probe.error_message << "\n";
                return probe_failure_exit_code();
            }
            if (!job.chapter_config_path.has_value()) {
                request.error_output << "Resolved batch job is missing a ChapterFile: " << job.source_path.string()
                                     << "\n";
                return CliExitCode::Error;
            }
            ChapterConfigLoadResult config = load_chapter_config(
                *job.chapter_config_path, probe.metadata.duration_ms, loaded_settings.export_settings);
            if (!config.ok()) {
                request.error_output << config.error_message << "\n";
                return CliExitCode::Error;
            }
            plan_inputs.push_back(OutputPlanInput {
                .metadata = probe.metadata,
                .chapter_source_path = job.chapter_config_path,
                .chapters = std::move(config.config.chapters),
                .settings = apply_cli_flag_overrides(std::move(config.config.settings), cli_arguments),
            });
        }
    }

    const OutputPlanResult output_plan = plan_outputs(plan_inputs);
    if (!output_plan.ok()) {
        for (const std::string& error : output_plan.errors) {
            request.error_output << error << "\n";
        }
        return CliExitCode::Error;
    }

    request.output << "Input: " << cli_arguments.input_paths.front().string() << "\n";
    if (cli_arguments.use_embedded_chapters) {
        request.output << "Config: --embedded\n";
    } else {
        request.output << "Config: " << cli_arguments.config_paths.front().string() << "\n";
    }
    request.output << "CLI settings: " << settings_paths.cli_settings_path.string() << "\n";
    request.output << "Settings loaded: CLI=" << (loaded_settings.loaded_cli_settings ? "yes" : "no");
    request.output << ", GUI=" << (loaded_settings.loaded_gui_settings ? "yes" : "no") << "\n";
    request.output << "Effective CRF: " << static_cast<int>(output_plan.jobs.front().settings.x264_crf) << "\n";

    if (cli_arguments.dry_run) {
        const DryRunRenderResult rendered = render_dry_run(output_plan.jobs,
            request.output,
            request.error_output,
            cli_arguments.aggregate_json_path,
            cli_arguments.aggregate_csv_path);
        return rendered.ok() ? CliExitCode::Success : CliExitCode::ExportFailure;
    }

    const ExportRunResult export_result = ExportRunner {request.process_executor}.run(output_plan.jobs,
        ExportRunOptions {
            .segment_started =
                [&request](const size_t job_index,
                    const size_t job_count,
                    const size_t chapter_index,
                    const size_t chapter_count,
                    const ResolvedExportJob& job,
                    const PlannedExportSegment& segment) {
                    request.output << "Exporting Job " << job_index << "/" << job_count << ", Chapter " << chapter_index
                                   << "/" << chapter_count << ": " << job.metadata.source_path.string() << " -> "
                                   << segment.output_path.string() << "\n";
                },
            .segment_finished =
                [&request](const RenderedSegment& segment) {
                    if (segment.ok()) {
                        request.output << "Completed Chapter " << segment.chapter_index + 1 << ": "
                                       << segment.output_path.string() << "\n";
                    } else {
                        request.error_output << "Failed Chapter " << segment.chapter_index + 1 << ": "
                                             << segment.output_path.string() << ": " << segment.process.error_message
                                             << "\n";
                    }
                },
            .message = [&request](const std::string& message) { request.output << message << "\n"; },
        });

    const ManifestWriteResult manifests = write_manifests(
        output_plan.jobs, export_result, cli_arguments.aggregate_json_path, cli_arguments.aggregate_csv_path);
    for (const Path& path : manifests.written_paths) {
        request.output << "Manifest: " << path.string() << "\n";
    }
    for (const std::string& error : manifests.errors) {
        request.error_output << error << "\n";
    }
    if (!manifests.ok()) {
        for (const Path& path : manifests.preserved_media_paths) {
            request.error_output << "Preserved rendered media: " << path.string() << "\n";
        }
    }

    auto exported = size_t {0};
    auto failed = size_t {0};
    auto skipped = size_t {0};
    auto overwritten = size_t {0};
    for (const ExportJobResult& job : export_result.jobs) {
        for (const RenderedSegment& segment : job.segments) {
            if (!segment.ok()) {
                ++failed;
            } else if (segment.skipped) {
                ++skipped;
            } else {
                ++exported;
            }
            if (segment.overwrote_existing) {
                ++overwritten;
            }
        }
    }
    request.output << "Summary: exported=" << exported << ", failed=" << failed << ", skipped=" << skipped
                   << ", overwritten=" << overwritten << "\n";
    if (!export_result.ok()) {
        return export_exit_code(export_result);
    }
    if (!manifests.ok()) {
        return CliExitCode::ExportFailure;
    }
    return CliExitCode::Success;
}

} // namespace vidchopper
