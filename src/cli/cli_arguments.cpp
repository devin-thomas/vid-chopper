#include "cli/cli_arguments.h"

#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

namespace vidchopper {

namespace {

using TokenList = std::vector<std::string>;

constexpr std::string_view command_chop {"chop"};
constexpr std::string_view flag_help {"--help"};
constexpr std::string_view short_flag_help {"-h"};
constexpr std::string_view flag_dry_run {"--dry-run"};
constexpr std::string_view flag_use_gui_config {"--use-gui-config"};
constexpr std::string_view flag_stop_on_first_error {"--stop-on-first-error"};
constexpr std::string_view flag_crf {"--crf"};
constexpr std::string_view flag_cq {"--cq"};
constexpr std::string_view flag_preset {"--preset"};
constexpr std::string_view flag_threads {"--threads"};

[[nodiscard]] auto success(CliArguments arguments) -> CliParseResult {
    return CliParseResult {
        .success = true,
        .arguments = std::move(arguments),
    };
}

[[nodiscard]] auto failure(std::string message) -> CliParseResult {
    return CliParseResult {
        .error_message = std::move(message),
    };
}

[[nodiscard]] auto parse_unsigned_value(const std::string_view text) -> std::optional<u16> {
    if (text.empty()) {
        return std::nullopt;
    }

    u32 value {0};
    const char* const first = text.data();
    const char* const last = text.data() + text.size();
    const std::from_chars_result result = std::from_chars(first, last, value);
    if (result.ec != std::errc {} || result.ptr != last || value > std::numeric_limits<u16>::max()) {
        return std::nullopt;
    }

    return static_cast<u16>(value);
}

[[nodiscard]] auto parse_u8_value(const std::string_view text, const u8 maximum) -> std::optional<u8> {
    const std::optional<u16> parsed = parse_unsigned_value(text);
    if (!parsed.has_value() || *parsed > maximum) {
        return std::nullopt;
    }

    return static_cast<u8>(*parsed);
}

[[nodiscard]] auto is_flag(const std::string_view token) -> bool {
    return token.starts_with('-');
}

[[nodiscard]] auto next_value(const TokenList& tokens, const usize index) -> std::optional<std::string_view> {
    const usize next_index = index + 1;
    if (next_index >= tokens.size() || is_flag(tokens[next_index])) {
        return std::nullopt;
    }

    return std::string_view {tokens[next_index]};
}

[[nodiscard]] auto append_positional(CliArguments& arguments, const std::string_view token) -> bool {
    if (arguments.input_paths.empty()) {
        arguments.input_paths.emplace_back(std::string {token});
        return true;
    }

    if (arguments.config_paths.empty()) {
        arguments.config_paths.emplace_back(std::string {token});
        return true;
    }

    return false;
}

} // namespace

auto CliParseResult::ok() const noexcept -> bool {
    return success;
}

auto parse_cli_arguments(const std::vector<std::string>& tokens) -> CliParseResult {
    CliArguments arguments {};

    if (tokens.empty()) {
        return success(arguments);
    }

    usize index {0};
    const std::string_view first_token {tokens[index]};
    if (first_token == flag_help || first_token == short_flag_help) {
        return success(arguments);
    }

    if (first_token == command_chop) {
        arguments.command = CliCommand::Chop;
        ++index;
    } else {
        arguments.command = CliCommand::Chop;
    }

    for (; index < tokens.size(); ++index) {
        const std::string_view token {tokens[index]};

        if (token == flag_help || token == short_flag_help) {
            arguments.command = CliCommand::Help;
            return success(arguments);
        }

        if (token == flag_dry_run) {
            arguments.dry_run = true;
            continue;
        }

        if (token == flag_use_gui_config) {
            arguments.use_gui_config = true;
            continue;
        }

        if (token == flag_stop_on_first_error) {
            arguments.stop_on_first_error = true;
            continue;
        }

        if (token == flag_crf || token == flag_cq || token == flag_threads || token == flag_preset) {
            const std::optional<std::string_view> value = next_value(tokens, index);
            if (!value.has_value()) {
                return failure("Missing value for " + std::string {token} + ".");
            }

            if (token == flag_preset) {
                arguments.preset = std::string {*value};
                ++index;
                continue;
            }

            const u8 maximum = token == flag_threads ? std::numeric_limits<u8>::max() : u8 {51};
            const std::optional<u8> parsed = parse_u8_value(*value, maximum);
            if (!parsed.has_value()) {
                return failure("Invalid numeric value for " + std::string {token} + ".");
            }

            if (token == flag_crf) {
                arguments.crf = *parsed;
            } else if (token == flag_cq) {
                arguments.cq = *parsed;
            } else {
                arguments.threads = *parsed;
            }

            ++index;
            continue;
        }

        if (is_flag(token)) {
            return failure("Unknown option: " + std::string {token});
        }

        if (!append_positional(arguments, token)) {
            return failure("Too many positional arguments. Expected an input video and one chapter config.");
        }
    }

    return success(std::move(arguments));
}

auto cli_usage() -> std::string {
    return "Usage:\n"
           "  VidChopperCLI.exe <input-video> <chapters.json|chapters.yaml> [options]\n"
           "  VidChopperCLI.exe chop <input-video> <chapters.json|chapters.yaml> [options]\n"
           "\n"
           "Phase 1 options:\n"
           "  --dry-run              Print the planned work without exporting.\n"
           "  --crf <0-51>           Override x264 CRF for this run.\n"
           "  --cq <0-51>            Override NVENC CQ for this run.\n"
           "  --preset <name>        Override encoder preset for this run.\n"
           "  --threads <0-255>      Override ffmpeg thread count for this run.\n"
           "  --stop-on-first-error  Stop batch execution after the first failed item.\n"
           "  --use-gui-config       Explicitly read VidChopper.ini in addition to VidChopperCLI.ini.\n"
           "  -h, --help             Show this help text.\n"
           "\n"
           "Configuration note:\n"
           "  CLI preferences live in VidChopperCLI.ini. GUI settings are never read unless\n"
           "  --use-gui-config is passed.\n";
}

} // namespace vidchopper
