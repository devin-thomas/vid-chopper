#include "services/encoder_capability.hpp"

#include "core/string_utils.hpp"

#include <format>
#include <string_view>
#include <utility>

namespace vidchopper {

namespace {

[[nodiscard]] auto null_output_path() -> Path {
#ifdef _WIN32
    return Path {"NUL"};
#else
    return Path {"/dev/null"};
#endif
}

[[nodiscard]] auto quote_command_argument(const std::string_view argument) -> std::string {
    auto quoted = std::string {"\""};
    for (const char character : argument) {
        if (character == '"' || character == '\\') {
            quoted.push_back('\\');
        }
        quoted.push_back(character);
    }
    quoted.push_back('"');
    return quoted;
}

[[nodiscard]] auto command_summary(const std::vector<std::string>& command) -> std::string {
    auto summary = std::string {};
    for (const std::string& argument : command) {
        if (!summary.empty()) {
            summary.push_back(' ');
        }
        summary += quote_command_argument(argument);
    }
    return summary;
}

[[nodiscard]] auto make_process_request(
    const std::vector<std::string>& command, const EncoderCapabilityOptions& options) -> ProcessRequest {
    return ProcessRequest {
        .executable = command.front(),
        .arguments = {command.begin() + 1, command.end()},
        .timeout = options.timeout,
        .stdout_limit_bytes = options.stdout_limit_bytes,
        .stderr_limit_bytes = options.stderr_limit_bytes,
        .stop_token = options.stop_token,
    };
}

[[nodiscard]] auto bounded_detail(const ProcessResult& process) -> std::string {
    constexpr auto detail_limit = size_t {512};
    std::string detail = process.error_message.empty() ? process.standard_error : process.error_message;
    if (detail.size() > detail_limit) {
        detail.resize(detail_limit);
        detail += "... [truncated]";
    }
    return detail;
}

[[nodiscard]] auto process_summary(const ProcessResult& process) -> std::string {
    auto summary = process_exit_state_name(process.state);
    if (process.state == ProcessExitState::NonzeroExit) {
        summary += std::format(" (exit code {})", process.exit_code);
    }
    const std::string detail = bounded_detail(process);
    if (!detail.empty()) {
        summary += ": " + detail;
    }
    return summary;
}

[[nodiscard]] auto minimal_encode_command(
    const ExportSettings& settings, const EncoderKind backend) -> std::vector<std::string> {
    const EncoderDescriptor descriptor = encoder_descriptor(backend);
    auto command = std::vector<std::string> {
        settings.ffmpeg_path,
        "-hide_banner",
        "-loglevel",
        "error",
        "-f",
        "lavfi",
        "-i",
        "color=c=black:s=16x16:r=1",
        "-frames:v",
        "1",
        "-an",
        "-c:v",
        std::string {descriptor.codec_name},
    };
    const std::vector<std::string> encoder_arguments = encoder_arguments_for(settings, backend);
    command.insert(command.end(), encoder_arguments.begin(), encoder_arguments.end());
    command.insert(command.end(), {"-f", "null", null_output_path().string()});
    return command;
}

[[nodiscard]] auto listing_command(const ExportSettings& settings) -> std::vector<std::string> {
    return {settings.ffmpeg_path, "-hide_banner", "-encoders"};
}

[[nodiscard]] auto contains_encoder(const ProcessResult& process, const std::string_view codec_name) -> bool {
    const std::string output = to_lower_copy(process.standard_output + "\n" + process.standard_error);
    return output.find(to_lower_copy(std::string {codec_name})) != std::string::npos;
}

[[nodiscard]] auto selection_platform(const EncoderEnvironment& environment) -> EncoderPlatform {
    return environment.platform == EncoderPlatform::Unknown ? current_encoder_platform() : environment.platform;
}

[[nodiscard]] auto automatic_candidate(
    const ExportSettings& settings, const EncoderEnvironment& environment) -> EncoderKind {
    if (!settings.auto_detect_gpu) {
        return EncoderKind::X264;
    }

    const EncoderPlatform platform = selection_platform(environment);
    const bool apple_silicon_videotoolbox = platform == EncoderPlatform::MacOs
        && (environment.has_hevc_videotoolbox_encoder || current_encoder_is_apple_silicon());
    if (apple_silicon_videotoolbox) {
        return EncoderKind::HevcVideoToolbox;
    }
    if (platform == EncoderPlatform::Windows || platform == EncoderPlatform::Linux) {
        return EncoderKind::HevcNvenc;
    }
    return EncoderKind::X264;
}

[[nodiscard]] auto capability_rejection(const EncoderCapabilityResult& result) -> std::string {
    if (!result.rejection_reason.empty()) {
        return result.rejection_reason;
    }
    return std::string {result.backend_display_name} + " is unavailable.";
}

} // namespace

auto EncoderCapabilityResult::available() const noexcept -> bool {
    return status == EncoderCapabilityStatus::Available;
}

auto EncoderCapabilityResult::ok() const noexcept -> bool {
    return available();
}

auto EncoderSelectionResult::ok() const noexcept -> bool {
    return success && error_message.empty();
}

EncoderCapabilityService::EncoderCapabilityService(ProcessExecutor executor)
    : executor_ {std::move(executor)} {
}

auto EncoderCapabilityService::test(const ExportSettings& settings,
    const EncoderKind backend,
    const EncoderEnvironment& environment,
    const EncoderCapabilityOptions& options) const -> EncoderCapabilityResult {
    const EncoderDescriptor descriptor = encoder_descriptor(backend);
    auto result = EncoderCapabilityResult {
        .tested_backend = backend,
        .backend_display_name = std::string {descriptor.display_name},
    };

    if (!encoder_kind_is_known(backend) || backend == EncoderKind::Auto) {
        result.status = EncoderCapabilityStatus::Unsupported;
        result.rejection_reason =
            "The requested encoder value is unknown; choose Auto, x264, HEVC NVENC, or HEVC VideoToolbox.";
        return result;
    }
    if (!descriptor.enabled_in_release) {
        result.status = EncoderCapabilityStatus::Unsupported;
        result.rejection_reason = "The requested encoder is not enabled in this release.";
        return result;
    }

    const EncoderPlatform platform = selection_platform(environment);
    if (!encoder_platform_eligible(descriptor, platform)) {
        result.status = EncoderCapabilityStatus::Unsupported;
        result.rejection_reason =
            std::format("{} is not eligible on {}.", descriptor.display_name, encoder_platform_name(platform));
        return result;
    }

    if (options.use_encoder_listing_prefilter) {
        const std::vector<std::string> command = listing_command(settings);
        result.listing_checked = true;
        result.listing_process = executor_(make_process_request(command, options));
        if (result.listing_process.ok()) {
            result.listed_by_ffmpeg = contains_encoder(result.listing_process, descriptor.codec_name);
            if (!result.listed_by_ffmpeg) {
                result.rejection_reason =
                    std::format("{} is not listed by the selected ffmpeg.", descriptor.codec_name);
                result.process_summary = process_summary(result.listing_process);
                return result;
            }
        }
    }

    result.command = minimal_encode_command(settings, backend);
    result.command_summary = command_summary(result.command);
    result.process = executor_(make_process_request(result.command, options));
    result.process_summary = process_summary(result.process);
    if (result.process.ok()) {
        result.status = EncoderCapabilityStatus::Available;
        result.rejection_reason.clear();
        return result;
    }

    result.rejection_reason = std::format(
        "{} ({}) minimal encode failed ({}).",
        descriptor.display_name,
        descriptor.codec_name,
        result.process_summary);
    return result;
}

auto EncoderCapabilityService::select(const ExportSettings& settings,
    const EncoderEnvironment& environment,
    const EncoderCapabilityOptions& options) const -> EncoderSelectionResult {
    const EncoderKind requested_kind = settings.encoder_kind;
    auto result = EncoderSelectionResult {
        .selection =
            EncoderSelection {
                .requested_kind = requested_kind,
                .resolved_kind = EncoderKind::X264,
            },
    };

    if (requested_kind != EncoderKind::Auto) {
        EncoderCapabilityResult capability = test(settings, requested_kind, environment, options);
        result.capability_results.push_back(capability);
        if (!capability.ok()) {
            result.error_message = "Export blocked: " + capability_rejection(capability)
                + " The stored encoder preference remains " + std::string {encoder_kind_name(requested_kind)} + ".";
            return result;
        }

        result.success = true;
        result.selection.resolved_kind = requested_kind;
        result.summary = std::format("Encoder capability passed: {}.", capability.backend_display_name);
        return result;
    }

    const EncoderKind candidate = automatic_candidate(settings, environment);
    EncoderCapabilityResult candidate_result = test(settings, candidate, environment, options);
    result.capability_results.push_back(candidate_result);
    if (candidate_result.ok()) {
        result.success = true;
        result.selection.resolved_kind = candidate;
        result.summary = std::format(
            "Auto resolved to {} after a successful capability test.", candidate_result.backend_display_name);
        return result;
    }

    const std::string candidate_reason = capability_rejection(candidate_result);
    if (candidate == EncoderKind::X264) {
        result.error_message = "Export blocked: Auto could not use x264. " + candidate_reason;
        return result;
    }

    EncoderCapabilityResult fallback_result = test(settings, EncoderKind::X264, environment, options);
    result.capability_results.push_back(fallback_result);
    if (!fallback_result.ok()) {
        result.error_message = "Export blocked: Auto hardware selection failed (" + candidate_reason
            + "), and x264 fallback also failed (" + capability_rejection(fallback_result) + ").";
        return result;
    }

    result.success = true;
    result.selection.resolved_kind = EncoderKind::X264;
    result.selection.used_fallback = true;
    result.selection.reason = candidate_reason;
    result.summary = "Auto hardware capability failed: " + candidate_reason + " Resolved to x264 before export.";
    return result;
}

} // namespace vidchopper
