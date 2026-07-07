#include "cli/cli_arguments.h"

#include <charconv>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

namespace vidchopper {

namespace {

constexpr auto command_chop = std::string_view {"chop"};
constexpr auto flag_help = std::string_view {"--help"};
constexpr auto short_flag_help = std::string_view {"-h"};
constexpr auto flag_dry_run = std::string_view {"--dry-run"};
constexpr auto flag_use_gui_config = std::string_view {"--use-gui-config"};
constexpr auto flag_stop_on_first_error = std::string_view {"--stop-on-first-error"};
constexpr auto flag_crf = std::string_view {"--crf"};
constexpr auto flag_cq = std::string_view {"--cq"};
constexpr auto flag_preset = std::string_view {"--preset"};
constexpr auto flag_threads = std::string_view {"--threads"};

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

    auto value = u32 {0};
    const auto* first = text.data();
    const auto* last = text.data() + text.size();
    const auto result = std::from_chars(first, last, value);
    if (result.ec != std::errc {} || result.ptr != last || value > std::numeric_limits<u16>::max()) {
        return std::nullopt;
    }

    return static_cast<u16>(value);
}

[[nodiscard]] auto parse_u8_value(const std::string_view text, const u8 maximum) -> std::optional<u8> {
    const auto parsed = parse_unsigned_value(text);
    if (!parsed.has_value() || *parsed > maximum) {
        return std::nullopt;
    }

    return static_cast<u8>(*parsed);
}

[[nodiscard]] auto is_flag(const std::string_view token) -> bool {
    return token.starts_with('-');
}

[[nodiscard]] auto next_value(
    const std::vector<std::string>& tokens, const usize index) -> std::optional<std::string_view> {
    const auto next_index = index + 1;
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
    auto arguments = CliArguments {};

    if (tokens.empty()) {
        return success(arguments);
    }

    auto index = usize {0};
    const auto first_token = std::string_view {tokens[index]};
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
        const auto token = std::string_view {tokens[index]};

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
            const auto value = next_value(tokens, index);
            if (!value.has_value()) {
                return failure("Missing value for " + std::string {token} + ".");
            }

            if (token == flag_preset) {
                arguments.preset = std::string {*value};
                ++index;
                continue;
            }

            const auto maximum = token == flag_threads ? std::numeric_limits<u8>::max() : u8 {51};
            const auto parsed = parse_u8_value(*value, maximum);
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
