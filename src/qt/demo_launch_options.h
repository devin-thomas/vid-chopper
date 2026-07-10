#pragma once

#include <charconv>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

namespace vidchopper {

enum class DemoScene {
    None = 0,
    Workspace = 1,
    WorkspaceLogs = 2,
    SettingsPrecision = 3,
};

struct DemoWindowSize {
    int width {0};
    int height {0};
};

struct DemoLaunchOptions {
    DemoScene scene {DemoScene::None};
    std::filesystem::path demo_source;
    std::optional<DemoWindowSize> window_size;
    std::filesystem::path demo_ready_file;

    [[nodiscard]] auto enabled() const noexcept -> bool {
        return scene != DemoScene::None;
    }
};

struct DemoLaunchOptionsParseResult {
    bool success {true};
    std::string error_message;
    DemoLaunchOptions options;
};

namespace detail {

[[nodiscard]] inline auto lower_copy(std::string_view value) -> std::string {
    auto lowered = std::string {};
    lowered.reserve(value.size());

    for (const auto character : value) {
        if (character >= 'A' && character <= 'Z') {
            lowered.push_back(static_cast<char>(character - 'A' + 'a'));
        } else {
            lowered.push_back(character);
        }
    }

    return lowered;
}

[[nodiscard]] inline auto parse_demo_scene(const std::string_view value) -> std::optional<DemoScene> {
    const auto lowered = lower_copy(value);
    if (lowered == "workspace") {
        return DemoScene::Workspace;
    }
    if (lowered == "workspace-logs") {
        return DemoScene::WorkspaceLogs;
    }
    if (lowered == "settings-precision") {
        return DemoScene::SettingsPrecision;
    }
    return std::nullopt;
}

[[nodiscard]] inline auto parse_window_size(const std::string_view value) -> std::optional<DemoWindowSize> {
    auto separator = value.find('x');
    if (separator == std::string_view::npos) {
        separator = value.find('X');
    }

    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    auto width = int {0};
    auto height = int {0};

    const auto width_part = value.substr(0, separator);
    const auto height_part = value.substr(separator + 1);

    const auto width_result = std::from_chars(width_part.data(), width_part.data() + width_part.size(), width);
    const auto height_result = std::from_chars(height_part.data(), height_part.data() + height_part.size(), height);

    if (width_result.ec != std::errc {} || width_result.ptr != width_part.data() + width_part.size()) {
        return std::nullopt;
    }

    if (height_result.ec != std::errc {} || height_result.ptr != height_part.data() + height_part.size()) {
        return std::nullopt;
    }

    if (width <= 0 || height <= 0) {
        return std::nullopt;
    }

    return DemoWindowSize {
        .width = width,
        .height = height,
    };
}

} // namespace detail

[[nodiscard]] inline auto parse_demo_launch_options(const int argc, char* argv[]) -> DemoLaunchOptionsParseResult {
    auto result = DemoLaunchOptionsParseResult {};
    auto saw_demo_argument = false;

    for (auto index = 1; index < argc; ++index) {
        const auto argument = std::string_view {argv[index]};
        if (argument.rfind("--demo-scene=", 0) == 0) {
            saw_demo_argument = true;
            const auto parsed_scene = detail::parse_demo_scene(argument.substr(13));
            if (!parsed_scene.has_value()) {
                result.success = false;
                result.error_message = "Unsupported value for --demo-scene.";
                return result;
            }

            result.options.scene = *parsed_scene;
            continue;
        }

        if (argument.rfind("--demo-source=", 0) == 0) {
            saw_demo_argument = true;
            result.options.demo_source = std::filesystem::path {std::string {argument.substr(14)}};
            continue;
        }

        if (argument.rfind("--window-size=", 0) == 0) {
            saw_demo_argument = true;
            const auto parsed_size = detail::parse_window_size(argument.substr(14));
            if (!parsed_size.has_value()) {
                result.success = false;
                result.error_message = "Invalid value for --window-size. Expected <width>x<height>.";
                return result;
            }

            result.options.window_size = *parsed_size;
            continue;
        }

        if (argument.rfind("--demo-ready-file=", 0) == 0) {
            saw_demo_argument = true;
            result.options.demo_ready_file = std::filesystem::path {std::string {argument.substr(18)}};
            continue;
        }
    }

    if (!saw_demo_argument) {
        return result;
    }

    if (result.options.scene == DemoScene::None) {
        result.success = false;
        result.error_message = "Demo launch arguments require --demo-scene.";
        return result;
    }

    if (result.options.demo_source.empty()) {
        result.success = false;
        result.error_message = "Demo launch arguments require --demo-source.";
        return result;
    }

    if (!result.options.demo_source.is_absolute()) {
        result.success = false;
        result.error_message = "--demo-source must be an absolute path.";
        return result;
    }

    if (result.options.demo_ready_file.empty()) {
        result.success = false;
        result.error_message = "Demo launch arguments require --demo-ready-file.";
        return result;
    }

    if (!result.options.demo_ready_file.is_absolute()) {
        result.success = false;
        result.error_message = "--demo-ready-file must be an absolute path.";
        return result;
    }

    return result;
}

} // namespace vidchopper
