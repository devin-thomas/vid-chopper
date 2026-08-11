#pragma once

#include "core/encoder_model.hpp"
#include "services/process_runner.hpp"

#include <chrono>
#include <stop_token>
#include <string>
#include <vector>

namespace vidchopper {

enum class EncoderCapabilityStatus : u8 {
    Unavailable = 0,
    Available = 1,
    Unsupported = 2,
};

struct EncoderCapabilityOptions {
    std::chrono::milliseconds timeout {10000};
    size_t stdout_limit_bytes {1024 * 1024};
    size_t stderr_limit_bytes {4096};
    std::stop_token stop_token;
    bool use_encoder_listing_prefilter {false};
};

struct EncoderCapabilityResult {
    EncoderCapabilityStatus status {EncoderCapabilityStatus::Unavailable};
    EncoderKind tested_backend {EncoderKind::Auto};
    std::string backend_display_name;
    std::vector<std::string> command;
    std::string command_summary;
    ProcessResult process;
    std::string process_summary;
    std::string rejection_reason;
    bool listing_checked {false};
    bool listed_by_ffmpeg {true};
    ProcessResult listing_process;

    [[nodiscard]] auto available() const noexcept -> bool;
    [[nodiscard]] auto ok() const noexcept -> bool;
};

struct EncoderSelectionResult {
    bool success {false};
    EncoderSelection selection;
    std::vector<EncoderCapabilityResult> capability_results;
    std::string summary;
    std::string error_message;

    [[nodiscard]] auto ok() const noexcept -> bool;
};

class EncoderCapabilityService final {
public:
    explicit EncoderCapabilityService(ProcessExecutor executor = run_process);

    [[nodiscard]] auto test(const ExportSettings& settings,
        EncoderKind backend,
        const EncoderEnvironment& environment,
        const EncoderCapabilityOptions& options = {}) const -> EncoderCapabilityResult;
    [[nodiscard]] auto select(const ExportSettings& settings,
        const EncoderEnvironment& environment,
        const EncoderCapabilityOptions& options = {}) const -> EncoderSelectionResult;

private:
    ProcessExecutor executor_;
};

} // namespace vidchopper
