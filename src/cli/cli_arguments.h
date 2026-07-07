#pragma once

#include "core/types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vidchopper {

enum class CliCommand : u8 {
    Chop = 0,
    Help = 1,
};

struct CliArguments {
    CliCommand command {CliCommand::Help};
    std::vector<std::filesystem::path> input_paths;
    std::vector<std::filesystem::path> config_paths;
    std::optional<u8> crf;
    std::optional<u8> cq;
    std::optional<u8> threads;
    std::string preset;
    bool dry_run {false};
    bool use_gui_config {false};
    bool stop_on_first_error {false};

    [[nodiscard]] auto operator==(const CliArguments&) const -> bool = default;
};

struct CliParseResult {
    bool success {false};
    CliArguments arguments;
    std::string error_message;

    [[nodiscard]] auto ok() const noexcept -> bool;
    [[nodiscard]] auto operator==(const CliParseResult&) const -> bool = default;
};

[[nodiscard]] auto parse_cli_arguments(const std::vector<std::string>& tokens) -> CliParseResult;
[[nodiscard]] auto cli_usage() -> std::string;

} // namespace vidchopper
